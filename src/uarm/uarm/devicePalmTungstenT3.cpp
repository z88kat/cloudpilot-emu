//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include <cstdio>

#include "cputil.h"
#include "device.h"
#include "i2cdev_TPS65010.h"
#include "mmiodev_W86L488.h"
#include "savestate/savestateAll.h"
#include "sspdev_TSC210x.h"

// The TSC2101 conversion engine is clocked from the PCM task (14.7 kHz with audio off,
// 44.3 kHz with audio on). Every fourth tick is plenty for the touch screen and battery ADC.
#define TSC_PERIODIC_DIVIDER 4

/*
    Palm Tungsten T3: PXA26x, TSC2101 (touch screen, ADC and audio codec) on SSP, TPS65010
    power management on I2C, W86L488 SD controller, 64MB RAM, 320x480 display.

    GPIO    NOTES
    0       keypad matrix col 0
    1       reset button
    2       SD card detect (active low)
    3       slider open
    8       W86L488 irq (active low)
    10, 11  keypad matrix col 1, 2
    12      hotsync button (active low)
    14      TPS65010 irq?
    19-22   keypad matrix row 0-3
    24      TSC2101 chip select
    33      keypad matrix row 4
    37      TSC2101 PINTDAV

    The debug UART is the HWUART.

    The W86L488 model only covers the command path, it neither returns card responses nor
    transfers data. SD cards are therefore not presented to the OS.
*/

namespace {
    class DeviceTungstenT3 : public Device {
       public:
        DeviceTungstenT3(struct SocPeriphs *sp, struct Keypad *kp);

        void PcmPeriodic() override {
            if (++tscPeriodicCounter < TSC_PERIODIC_DIVIDER) return;

            tscPeriodicCounter = 0;
            tsc210xPeriodic(tsc2101);
        }

        void Touch(int x, int y) override {
            // The real hardware produces X values between 3728 (x = 0) and 300 (x = 319) and Y
            // values between 3793 (y = 0) and 153 (y = 479). Integer slopes keep the mapping
            // exactly invertible, see deviceGetPenTranslation.
            const bool down = x >= 0 && y >= 0;

            tsc210xPenInput(tsc2101, down ? 3713 - 10 * x : -1, down ? 3793 - 7 * y : -1);
        }

        bool UsesPxaMmc() const override { return false; }

        void SetSdCard(struct VSD *vsd) override {
            if (vsd) fprintf(stderr, "SD cards are not supported on the Tungsten T3\n");
        }

        void Save(Savestate<ChunkType> &savestate) override { DoSave(savestate); }
        void Save(SavestateProbe<ChunkType> &savestate) override { DoSave(savestate); }

        void Load(SavestateLoader<ChunkType> &loader) override {
            tsc210xLoad(tsc2101, loader);
            tps65010Load(tps65010, loader);
            w86l488Load(w86l488, loader);
        }

       private:
        template <typename T>
        void DoSave(T &savestate) {
            tsc210xSave(tsc2101, savestate);
            tps65010Save(tps65010, savestate);
            w86l488Save(w86l488, savestate);
        }

       private:
        struct Tsc210x *tsc2101{nullptr};
        struct Tps65010 *tps65010{nullptr};
        struct W86L488 *w86l488{nullptr};

        uint32_t tscPeriodicCounter{0};
    };

    DeviceTungstenT3::DeviceTungstenT3(struct SocPeriphs *sp, struct Keypad *kp)
        : Device(DeviceType5::deviceTypeTungstenT3) {
        tsc2101 = tsc210xInitSsp(sp->ssp, sp->gpio, 24, 37, TscType2101);
        if (!tsc2101) ERR("Cannot init TSC2101");

        tps65010 = tps65010Init(sp->i2c);
        if (!tps65010) ERR("Cannot init TPS65010");

        w86l488 = w86l488init(sp->mem, sp->gpio, W86L488_BASE_T3, nullptr, 8);
        if (!w86l488) ERR("Cannot init W86L488");

        pxaGpioSetState(sp->gpio, 1, true);   // reset button
        pxaGpioSetState(sp->gpio, 2, true);   // no SD card
        pxaGpioSetState(sp->gpio, 3, true);   // slider open
        pxaGpioSetState(sp->gpio, 12, true);  // hotsync button
        pxaGpioSetState(sp->gpio, 14, true);  // TPS65010 irq?

        // full battery
        tsc210xSetExtAdc(tsc2101, TscExternalAdcBat1, 4100);

        if (!keypadDefineCol(kp, 0, 0) || !keypadDefineCol(kp, 1, 10) ||
            !keypadDefineCol(kp, 2, 11))
            ERR("Cannot init keypad cols");

        static const int8_t rowGpios[] = {19, 20, 21, 22, 33};
        for (unsigned i = 0; i < sizeof(rowGpios); i++)
            if (!keypadDefineRow(kp, i, rowGpios[i])) ERR("Cannot init keypad rows");

        // The voice recorder button (row 3, col 0) is not mapped.
        if (!keypadAddMatrixKey(kp, keyIdHard1, 4, 0) ||  // datebook
            !keypadAddMatrixKey(kp, keyIdHard2, 0, 0) ||  // address
            !keypadAddMatrixKey(kp, keyIdHard3, 2, 2) ||  // todo
            !keypadAddMatrixKey(kp, keyIdHard4, 1, 2) ||  // memos
            !keypadAddMatrixKey(kp, keyIdDown, 0, 1) || !keypadAddMatrixKey(kp, keyIdUp, 4, 1) ||
            !keypadAddMatrixKey(kp, keyIdLeft, 1, 1) ||
            !keypadAddMatrixKey(kp, keyIdRight, 2, 1) ||
            !keypadAddMatrixKey(kp, keyIdSelect, 3, 2))
            ERR("Cannot init keypad keys");

        sp->dbgUart = sp->uarts[1];  // HWUART
    }
}  // namespace

struct Device *deviceSetupTungstenT3(struct SocPeriphs *sp, struct Keypad *kp) {
    return new DeviceTungstenT3(sp, kp);
}
