#include "device.h"

#include "cputil.h"
#include "patches.h"

enum RamTermination deviceGetRamTerminationStyle(enum DeviceType5 type) {
    switch (type) {
        case DeviceType5::deviceTypeTungstenC:
            // the TC probes for RAM size by writing past the end of RAM
            return RamTerminationWriteIgnore;

        default:
            return RamTerminationMirror;
    }
}

uint_fast8_t deviceGetSocRev(enum DeviceType5 type) {
    switch (type) {
        case DeviceType5::deviceTypeTungstenT3:
            return 1;  // PXA26x

        default:
            return 0;  // PXA25x
    }
}

bool deviceI2sConnected(enum DeviceType5 type) {
    // the T3 streams audio to the TSC2101 over I2S; the I2S unit must drain its FIFO or the OS
    // stalls on playback
    return type == DeviceType5::deviceTypeTungstenT3;
}

const struct PenTranslation *deviceGetPenTranslation(enum DeviceType5 type) {
    // These must match the mappings in the Touch() methods of the devices, modulo whatever
    // transformation the HAL applies before handing the coordinates to PenRawToScreen.
    static const PenTranslation penTranslationE2 = {
        .x = {.rawOffset = 280, .screenOffset = 0, .num = 10, .den = 1},
        .y = {.rawOffset = 210, .screenOffset = 0, .num = 7, .den = 1},
    };

    // The UCB1400 driver inverts Y: raw = 0x3ff - (960 - 2 * y)
    static const PenTranslation penTranslationTC = {
        .x = {.rawOffset = 320, .screenOffset = 0, .num = 2, .den = 1},
        .y = {.rawOffset = 63, .screenOffset = 0, .num = 2, .den = 1},
    };

    // see DeviceTungstenT3::Touch
    static const PenTranslation penTranslationT3 = {
        .x = {.rawOffset = 3713, .screenOffset = 0, .num = -10, .den = 1},
        .y = {.rawOffset = 3793, .screenOffset = 0, .num = -7, .den = 1},
    };

    switch (type) {
        case DeviceType5::deviceTypeE2:
        case DeviceType5::deviceTypeFrankenE2:
            return &penTranslationE2;

        case DeviceType5::deviceTypeTungstenC:
            return &penTranslationTC;

        case DeviceType5::deviceTypeTungstenT3:
            return &penTranslationT3;

        default:
            return nullptr;
    }
}

struct Device *deviceSetup(enum DeviceType5 type, struct SocPeriphs *sp,
                           struct Reschedule reschedule, struct GetEmuTime getEmuTime,
                           struct Keypad *kp, struct VSD *vsd, uint8_t *nandContent,
                           size_t nandSize, const struct MemoryBuffer *nandPageBuffer) {
    switch (type) {
        case DeviceType5::deviceTypeE2:
        case DeviceType5::deviceTypeFrankenE2:
            return deviceSetupTungstenE2(type, sp, reschedule, getEmuTime, kp, vsd, nandContent,
                                         nandSize, nandPageBuffer);

        case DeviceType5::deviceTypeTungstenC:
            return deviceSetupTungstenC(sp, getEmuTime, kp);

        case DeviceType5::deviceTypeTungstenT3:
            return deviceSetupTungstenT3(sp, kp);

        default:
            ERR("unsupported device type %i", (int)type);
    }
}
