//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include <cstring>
#include <deque>

#include "ac97dev_UCB1400.h"
#include "cputil.h"
#include "device.h"
#include "savestate/savestateAll.h"

#define SAVESTATE_VERSION 0

#define WIFI_MEM_SIZE 1024
#define WIFI_MEM1_BASE 0x28000000ul
#define WIFI_MEM2_BASE 0x20000000ul

// matrix positions of the modifier keys
#define KEY_FUNCTION_ROW 11
#define KEY_FUNCTION_COL 3
#define KEY_COMMAND_ROW 1
#define KEY_COMMAND_COL 2
#define KEY_SHIFT_ROW 6
#define KEY_SHIFT_COL 0

// Delay between synthesized key matrix transitions (typed characters and the menu key). The
// OS drops keys if the matrix changes faster.
#define KEY_STEP_NSEC 40000000ull

// Upper bound for the synthesized key matrix transitions waiting to be played back. Typed
// characters are dropped if the queue is full.
#define MAX_PENDING_KEY_STEPS 1024

/*
    Palm Tungsten C: PXA255, UCB1400 (AC97 codec + touch screen + ADC), 64MB RAM, 320x320
    display without a graffiti area, SD slot on the PXA MMC controller.

    GPIO    NOTES
    0       keypad matrix col 0
    1       reset button
    3       UCB1400 irq
    7       hotsync button (active low)
    9-11    keypad matrix col 1-3
    12      SD card detect (active low)
    13      wifi ready (active low)
    14      wifi power (active low)
    18-27   keypad matrix row 0-9
    79, 80  keypad matrix row 10, 11

    KEY MATRIX (row, col)
    0, 0-3  application buttons: date book, address, versamail, web
    1, 0-3  x, 5-way down, command, 5-way select
    2       unused
    3, 0-3  tab, d, h, period
    4, 0-3  a, c, y, l
    5, 0-3  q, f, n, o
    6, 0-3  shift, r, j, home
    7, 0-3  z, space, u, return
    8, 0-3  s, v, m, backspace
    9, 0-3  w, g, k, p
    10, 0-3 e, t, i, b
    11, 0-3 5-way up, left, right, function

    The function layer of the letter keys carries the digits and most punctuation, see
    functionLayer below. Function + space opens the on-screen symbol keyboard.

    UCB1400 GPIO0: external power (active high)
    UCB1400 GPIO1: headphones (active high)
    UCB1400 GPIO3: speaker enable (active high)
    UCB1400 GPIO5: vibrate (active high)
    UCB1400 GPIO7: LED (active low)
    UCB1400 AUX0: battery voltage
*/

namespace {
    class DeviceTungstenC : public Device {
       public:
        DeviceTungstenC(struct SocPeriphs *sp, struct GetEmuTime getEmuTime, struct Keypad *kp);

        void Key(uint32_t key, bool down) override {
            // There is no menu button on the TC. The menu is opened by function + command,
            // and the OS only recognizes the combination if function goes down first.
            if (key == keyIdMenu && down)
                QueueKeyStroke(MatrixKey{KEY_COMMAND_ROW, KEY_COMMAND_COL, Modifier::function});
        }

        bool HasKeyboard() const override { return true; }

        void TypeChar(uint8_t ch) override;

        void PcmPeriodic() override {
            ucb1400periodic(ucb1400);
            if (!keySteps.empty()) PlayKeySteps();
        }

        void Touch(int x, int y) override {
            const bool down = x >= 0 && y >= 0;

            ucb1400setPen(ucb1400, down ? 320 + 2 * x : -1, down ? 960 - 2 * y : -1);
        }

        void SetAudioQueue(struct AudioQueue *audioQueue) override {
            ucb1400setAudioQueue(ucb1400, audioQueue);
        }

        void SetSdCard(struct VSD *vsd) override { pxaGpioSetState(gpio, 12, !vsd); }

        void Save(Savestate<ChunkType> &savestate) override { DoSave(savestate); }
        void Save(SavestateProbe<ChunkType> &savestate) override { DoSave(savestate); }

        void Load(SavestateLoader<ChunkType> &loader) override {
            ucb1400Load(ucb1400, loader);
            AbortKeySteps();

            auto chunk =
                loader.GetChunkOrFail(ChunkType::deviceTungstenC, SAVESTATE_VERSION, "TC device");
            if (!chunk) return;

            LoadChunkHelper helper(*chunk);
            DoSaveLoad(helper);
        }

       private:
        template <typename T>
        void DoSave(T &savestate) {
            ucb1400Save(ucb1400, savestate);

            auto chunk = savestate.GetChunk(ChunkType::deviceTungstenC, SAVESTATE_VERSION);
            if (!chunk) ERR("unable to allocate chunk");

            SaveChunkHelper helper(*chunk);
            DoSaveLoad(helper);
        }

        template <typename T>
        void DoSaveLoad(T &chunkHelper) {
            chunkHelper.DoBuffer(wifiMem1, WIFI_MEM_SIZE).DoBuffer(wifiMem2, WIFI_MEM_SIZE);
        }

        static bool WifiMemAccess(void *userData, uint32_t pa, uint_fast8_t size, bool write,
                                  void *buf);

        uint64_t GetTime() { return getEmuTime.getTimeF(getEmuTime.userdata); }

        enum class Modifier : uint8_t { none, shift, function };

        struct MatrixKey {
            uint8_t row;
            uint8_t col;
            Modifier modifier;
        };

        struct KeyStep {
            uint8_t row;
            uint8_t col;
            bool down;
        };

        static bool LookupChar(uint8_t ch, MatrixKey &key);

        // Typed characters and the menu key are played back as a sequence of key matrix
        // transitions with KEY_STEP_NSEC between them.
        void QueueKeyStroke(const MatrixKey &key);
        void PlayKeySteps();
        void AbortKeySteps();
        void SetMatrixKey(uint8_t row, uint8_t col, bool down);

       private:
        struct UCB1400 *ucb1400{nullptr};
        struct PxaGpio *gpio{nullptr};
        struct Keypad *kp{nullptr};
        struct GetEmuTime getEmuTime;

        std::deque<KeyStep> keySteps;
        uint64_t nextKeyStepAt{0};
        // matrix positions held down by the key step player, bit 4 * row + col
        uint64_t keyStepsPressed{0};

        // the wifi module is not emulated, but the OS expects these two windows to be
        // backed by memory
        uint8_t wifiMem1[WIFI_MEM_SIZE] = {};
        uint8_t wifiMem2[WIFI_MEM_SIZE] = {};
    };

    bool DeviceTungstenC::WifiMemAccess(void *userData, uint32_t pa, uint_fast8_t size,
                                        bool write, void *buf) {
        auto *mem = reinterpret_cast<uint8_t *>(userData);
        const uint32_t offset = pa & (WIFI_MEM_SIZE - 1);

        if (offset + size > WIFI_MEM_SIZE) return false;

        if (write)
            memcpy(mem + offset, buf, size);
        else
            memcpy(buf, mem + offset, size);

        return true;
    }

    // keyboard matrix positions of the letters a - z
    const uint8_t letterKeys[26][2] = {
        {4, 0}, {10, 3}, {4, 1}, {3, 1}, {10, 0}, {5, 1}, {9, 1}, {3, 2}, {10, 2},
        {6, 2}, {9, 2},  {4, 3}, {8, 2}, {5, 2},  {5, 3}, {9, 3}, {5, 0}, {6, 1},
        {8, 0}, {10, 1}, {7, 2}, {8, 1}, {9, 0},  {1, 0}, {4, 2}, {7, 0},
    };

    // characters on the function layer and the letter that carries them
    const char functionLayer[][2] = {
        {'1', 'q'}, {'2', 'w'}, {'3', 'e'}, {'4', 'r'}, {'5', 't'},  {'6', 'y'}, {'7', 'u'},
        {'8', 'i'}, {'9', 'o'}, {'0', 'p'}, {':', 'a'}, {'!', 'b'},  {')', 'c'}, {'+', 'd'},
        {'-', 'f'}, {'*', 'g'}, {'_', 'h'}, {'"', 'j'}, {'\'', 'k'}, {'@', 'l'}, {'?', 'm'},
        {',', 'n'}, {'/', 's'}, {'#', 'v'}, {'$', 'z'}, {'(', 'x'},
    };

    bool DeviceTungstenC::LookupChar(uint8_t ch, MatrixKey &key) {
        auto letter = [&](uint8_t c, Modifier modifier) {
            key = {letterKeys[c - 'a'][0], letterKeys[c - 'a'][1], modifier};
            return true;
        };

        if (ch >= 'a' && ch <= 'z') return letter(ch, Modifier::none);
        if (ch >= 'A' && ch <= 'Z') return letter(ch - 'A' + 'a', Modifier::shift);

        for (auto &mapping : functionLayer)
            if (mapping[0] == ch) return letter(mapping[1], Modifier::function);

        switch (ch) {
            case '.':
                key = {3, 3, Modifier::none};
                return true;

            case ' ':
                key = {7, 1, Modifier::none};
                return true;

            case '\t':
                key = {3, 0, Modifier::none};
                return true;

            case '\r':
            case '\n':
                key = {7, 3, Modifier::none};
                return true;

            case '\b':
                key = {8, 3, Modifier::none};
                return true;

            default:
                return false;
        }
    }

    void DeviceTungstenC::TypeChar(uint8_t ch) {
        MatrixKey key;
        if (LookupChar(ch, key)) QueueKeyStroke(key);
    }

    void DeviceTungstenC::QueueKeyStroke(const MatrixKey &key) {
        uint8_t modifierRow = 0, modifierCol = 0;
        bool hasModifier = true;

        switch (key.modifier) {
            case Modifier::shift:
                modifierRow = KEY_SHIFT_ROW;
                modifierCol = KEY_SHIFT_COL;
                break;

            case Modifier::function:
                modifierRow = KEY_FUNCTION_ROW;
                modifierCol = KEY_FUNCTION_COL;
                break;

            default:
                hasModifier = false;
                break;
        }

        // If the player is idle, playback starts once KEY_STEP_NSEC have passed since the last
        // transition (nextKeyStepAt is never ahead of the current time by more than that).
        if (keySteps.size() + 4 > MAX_PENDING_KEY_STEPS) return;

        if (hasModifier) keySteps.push_back({modifierRow, modifierCol, true});
        keySteps.push_back({key.row, key.col, true});
        keySteps.push_back({key.row, key.col, false});
        if (hasModifier) keySteps.push_back({modifierRow, modifierCol, false});
    }

    void DeviceTungstenC::PlayKeySteps() {
        if (GetTime() < nextKeyStepAt) return;

        const KeyStep step = keySteps.front();
        keySteps.pop_front();

        SetMatrixKey(step.row, step.col, step.down);
        nextKeyStepAt = GetTime() + KEY_STEP_NSEC;
    }

    void DeviceTungstenC::AbortKeySteps() {
        keySteps.clear();
        // emulated time may have jumped backwards
        nextKeyStepAt = 0;

        for (uint8_t i = 0; i < 64; i++)
            if (keyStepsPressed & (1ull << i)) SetMatrixKey(i >> 2, i & 0x03, false);
    }

    void DeviceTungstenC::SetMatrixKey(uint8_t row, uint8_t col, bool down) {
        const uint64_t mask = 1ull << (4 * row + col);
        keyStepsPressed = down ? keyStepsPressed | mask : keyStepsPressed & ~mask;

        keypadMatrixKeyEvt(kp, row, col, down);
    }

    DeviceTungstenC::DeviceTungstenC(struct SocPeriphs *sp, struct GetEmuTime getEmuTime,
                                     struct Keypad *kp)
        : Device(DeviceType5::deviceTypeTungstenC),
          gpio(sp->gpio),
          kp(kp),
          getEmuTime(getEmuTime) {
        ucb1400 = ucb1400Init(sp->ac97, sp->gpio, 3);
        if (!ucb1400) ERR("Cannot init UCB1400");

        pxaGpioSetState(sp->gpio, 1, true);  // reset button
        pxaGpioSetState(sp->gpio, 7, true);  // hotsync button

        if (!keypadDefineCol(kp, 0, 0) || !keypadDefineCol(kp, 1, 9) ||
            !keypadDefineCol(kp, 2, 10) || !keypadDefineCol(kp, 3, 11))
            ERR("Cannot init keypad cols");

        static const int8_t rowGpios[] = {18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 79, 80};
        for (unsigned i = 0; i < sizeof(rowGpios); i++)
            if (!keypadDefineRow(kp, i, rowGpios[i])) ERR("Cannot init keypad rows");

        // The hardware buttons and the home key are mapped to key IDs. The rest of the keyboard
        // is driven by TypeChar, and keyIdMenu is synthesized as function + command, see Key.
        if (!keypadAddMatrixKey(kp, keyIdHard1, 0, 0) ||  // date book
            !keypadAddMatrixKey(kp, keyIdHard2, 0, 1) ||  // address
            !keypadAddMatrixKey(kp, keyIdHard3, 0, 2) ||  // versamail
            !keypadAddMatrixKey(kp, keyIdHard4, 0, 3) ||  // web
            !keypadAddMatrixKey(kp, keyIdUp, 11, 0) || !keypadAddMatrixKey(kp, keyIdDown, 1, 1) ||
            !keypadAddMatrixKey(kp, keyIdLeft, 11, 1) ||
            !keypadAddMatrixKey(kp, keyIdRight, 11, 2) ||
            !keypadAddMatrixKey(kp, keyIdSelect, 1, 3) ||
            !keypadAddMatrixKey(kp, keyIdHome, 6, 3))
            ERR("Cannot init keypad keys");

        if (!memRegionAdd(sp->mem, WIFI_MEM1_BASE, WIFI_MEM_SIZE, WifiMemAccess, wifiMem1) ||
            !memRegionAdd(sp->mem, WIFI_MEM2_BASE, WIFI_MEM_SIZE, WifiMemAccess, wifiMem2))
            ERR("Cannot init wifi memory windows");

        pxaGpioSetState(sp->gpio, 13, true);  // wifi not ready
        pxaGpioSetState(sp->gpio, 14, true);  // wifi not powered

        SetSdCard(nullptr);

        ucb1400setGpioInputVal(ucb1400, 0, false);  // external power is not on
        ucb1400setGpioInputVal(ucb1400, 1, false);  // no headphones plugged in

        // AUX2 and 3 appear to be dock related, AUX0 is battery voltage
        ucb1400setAuxVoltage(ucb1400, 0, 3800);

        sp->dbgUart = sp->uarts[0];  // FFUART
    }
}  // namespace

struct Device *deviceSetupTungstenC(struct SocPeriphs *sp, struct GetEmuTime getEmuTime,
                                    struct Keypad *kp) {
    return new DeviceTungstenC(sp, getEmuTime, kp);
}
