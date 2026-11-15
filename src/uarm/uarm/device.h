//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <cstdint>
#include <cstdio>

#include "SoC.h"
#include "device_type5.h"
#include "get_emu_time.h"
#include "keys.h"
#include "mem.h"
#include "nand.h"
#include "pxa_AC97.h"
#include "pxa_GPIO.h"
#include "pxa_I2C.h"
#include "pxa_I2S.h"
#include "pxa_SSP.h"
#include "pxa_UART.h"
#include "reschedule.h"
#include "savestate/savestateAll.h"
#include "vSD.h"

#define DEVICE_PERIODIC_TIER0 0
#define DEVICE_PERIODIC_TIER1 1

struct AudioQueue;

struct SocPeriphs {
    // in to deviceSetup
    struct PxaAC97 *ac97;
    struct PxaGpio *gpio;
    struct PxaI2c *i2c;
    struct PxaI2s *i2s;
    struct PxaSsp *ssp;
    struct PxaSsp *ssp2;  // assp for xscale
    struct PxaSsp *ssp3;  // nssp for scale
    struct ArmMem *mem;
    class SoC *soc;

    // PXA order: ffUart, hwUart, stUart, btUart
    struct PxaUart *uarts[4];

    void *adc;  // some cases need this
    void *kpc;  // some cases need this

    // out from deviceSetup
    struct NAND *nand;
    struct PxaUart *dbgUart;
    struct PxaUart *btUart;
};

enum RamTermination {  // what's after ram in phys map? (some devices probe)
    RamTerminationMirror,
    RamTerminationWriteIgnore,
    RamTerminationNone,
};

struct MemoryBuffer;
struct PenTranslation;

// Base class for the emulated handhelds. Each device lives in its own devicePalm*.cpp and is
// created by deviceSetup().
struct Device {
    explicit Device(enum DeviceType5 type) : type(type) {}
    virtual ~Device() = default;

    enum DeviceType5 GetType() const { return type; }

    virtual void Key(uint32_t key, bool down) {}
    // Devices with a hardware keyboard translate typed characters into key presses.
    virtual bool HasKeyboard() const { return false; }
    virtual void TypeChar(uint8_t ch) {}
    virtual void Periodic(uint32_t tier) {}
    virtual void PcmPeriodic() {}
    virtual void Touch(int x, int y) = 0;
    virtual bool TaskRequired(uint32_t tier) { return tier != DEVICE_PERIODIC_TIER0; }
    virtual void SetAudioQueue(struct AudioQueue *audioQueue) {}

    // true if the SD card is wired to the PXA MMC controller (the default)
    virtual bool UsesPxaMmc() const { return true; }
    // vsd is nullptr if no card is inserted
    virtual void SetSdCard(struct VSD *vsd) {}

    virtual void Save(Savestate<ChunkType> &savestate) = 0;
    virtual void Save(SavestateProbe<ChunkType> &savestate) = 0;
    virtual void Load(SavestateLoader<ChunkType> &loader) = 0;

   private:
    enum DeviceType5 type;
};

// simple queries
enum RamTermination deviceGetRamTerminationStyle(enum DeviceType5 type);
uint_fast8_t deviceGetSocRev(enum DeviceType5 type);
bool deviceI2sConnected(enum DeviceType5 type);

// Mapping between the raw pen coordinates seen by the OS and screen coordinates. Used to
// bypass digitizer calibration.
const struct PenTranslation *deviceGetPenTranslation(enum DeviceType5 type);

// device handling
struct Device *deviceSetup(enum DeviceType5 type, struct SocPeriphs *sp,
                           struct Reschedule reschedule, struct GetEmuTime getEmuTime,
                           struct Keypad *kp, struct VSD *vsd, uint8_t *nandContent,
                           size_t nandSize, const struct MemoryBuffer *nandPageBuffer);

// per-device constructors, see devicePalm*.cpp
struct Device *deviceSetupTungstenE2(enum DeviceType5 type, struct SocPeriphs *sp,
                                     struct Reschedule reschedule, struct GetEmuTime getEmuTime,
                                     struct Keypad *kp, struct VSD *vsd, uint8_t *nandContent,
                                     size_t nandSize, const struct MemoryBuffer *nandPageBuffer);
struct Device *deviceSetupTungstenC(struct SocPeriphs *sp, struct GetEmuTime getEmuTime,
                                    struct Keypad *kp);
struct Device *deviceSetupTungstenT3(struct SocPeriphs *sp, struct Keypad *kp);

inline void deviceKey(struct Device *dev, uint32_t key, bool down) { dev->Key(key, down); }
inline bool deviceHasKeyboard(struct Device *dev) { return dev->HasKeyboard(); }
inline void deviceTypeChar(struct Device *dev, uint8_t ch) { dev->TypeChar(ch); }
inline void devicePeriodic(struct Device *dev, uint32_t tier) { dev->Periodic(tier); }
inline void devicePcmPeriodic(struct Device *dev) { dev->PcmPeriodic(); }
inline void deviceTouch(struct Device *dev, int x, int y) { dev->Touch(x, y); }

inline bool deviceTaskRequired(struct Device *dev, uint32_t tier) {
    return dev->TaskRequired(tier);
}

inline void deviceSetAudioQueue(struct Device *dev, struct AudioQueue *audioQueue) {
    dev->SetAudioQueue(audioQueue);
}

inline bool deviceUsesPxaMmc(struct Device *dev) { return dev->UsesPxaMmc(); }
inline void deviceSetSdCard(struct Device *dev, struct VSD *vsd) { dev->SetSdCard(vsd); }

inline enum DeviceType5 deviceGetType(struct Device *dev) { return dev->GetType(); }

template <typename T>
void deviceSave(struct Device *dev, T &savestate) {
    dev->Save(savestate);
}

template <typename T>
void deviceLoad(struct Device *dev, T &loader) {
    dev->Load(loader);
}

#endif
