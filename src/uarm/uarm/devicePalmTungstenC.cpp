//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include <cstring>

#include "ac97dev_UCB1400.h"
#include "cputil.h"
#include "device.h"
#include "savestate/savestateAll.h"

#define SAVESTATE_VERSION 0

#define WIFI_MEM_SIZE 1024
#define WIFI_MEM1_BASE 0x28000000ul
#define WIFI_MEM2_BASE 0x20000000ul

// matrix positions of the function and command keys
#define KEY_FUNCTION_ROW 11
#define KEY_FUNCTION_COL 3
#define KEY_COMMAND_ROW 1
#define KEY_COMMAND_COL 2

// delay between the steps of the synthesized menu key sequence
#define MENU_SEQUENCE_STEP_NSEC 50000000ull

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

    KEY MATRIX (row, col; the remaining positions are the keyboard)
    0, 0-3  application buttons: date book, address, versamail, web
    1, 1    5-way down
    1, 2    command
    1, 3    5-way select
    6, 3    home
    11, 0-2 5-way up, left, right
    11, 3   function

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
            if (key == keyIdMenu && down && menuSequenceStep == 0) StartMenuSequence();
        }

        void PcmPeriodic() override {
            ucb1400periodic(ucb1400);
            if (menuSequenceStep != 0) AdvanceMenuSequence();
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
            AbortMenuSequence();

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

        // There is no menu button on the TC. The menu is opened by function + command, and
        // the OS only recognizes the combination if function goes down first.
        void StartMenuSequence();
        void AdvanceMenuSequence();
        void AbortMenuSequence();

       private:
        struct UCB1400 *ucb1400{nullptr};
        struct PxaGpio *gpio{nullptr};
        struct Keypad *kp{nullptr};
        struct GetEmuTime getEmuTime;

        uint32_t menuSequenceStep{0};
        uint64_t menuSequenceNextStepAt{0};

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

    void DeviceTungstenC::StartMenuSequence() {
        keypadMatrixKeyEvt(kp, KEY_FUNCTION_ROW, KEY_FUNCTION_COL, true);

        menuSequenceStep = 1;
        menuSequenceNextStepAt = GetTime() + MENU_SEQUENCE_STEP_NSEC;
    }

    void DeviceTungstenC::AdvanceMenuSequence() {
        if (GetTime() < menuSequenceNextStepAt) return;

        switch (menuSequenceStep) {
            case 1:
                keypadMatrixKeyEvt(kp, KEY_COMMAND_ROW, KEY_COMMAND_COL, true);
                break;

            case 2:
                keypadMatrixKeyEvt(kp, KEY_COMMAND_ROW, KEY_COMMAND_COL, false);
                break;

            default:
                AbortMenuSequence();
                return;
        }

        menuSequenceStep++;
        menuSequenceNextStepAt += MENU_SEQUENCE_STEP_NSEC;
    }

    void DeviceTungstenC::AbortMenuSequence() {
        keypadMatrixKeyEvt(kp, KEY_COMMAND_ROW, KEY_COMMAND_COL, false);
        keypadMatrixKeyEvt(kp, KEY_FUNCTION_ROW, KEY_FUNCTION_COL, false);

        menuSequenceStep = 0;
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

        // Only the hardware buttons are mapped, the keyboard is not. The TC has no dedicated
        // home or menu button: home is a key on the keyboard, and keyIdMenu is synthesized as
        // function + command, see StartMenuSequence.
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
