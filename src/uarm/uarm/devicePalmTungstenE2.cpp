//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include <cstdlib>

#include "ac97dev_WM9712L.h"
#include "cputil.h"
#include "device.h"
#include "device_type5.h"
#include "mmiodev_DirectNAND.h"
#include "savestate/savestateAll.h"
#include "uartdev_bcm2035.h"

// clang-format off
/*
		GPIO	DIR	VAL	AFR	EDGE	NOTES
		0		IN		0
		1		IN		1			gpio reset
		2		IN		0
		3		IN		0
		4		IN		0	FE 		checked in FIQ handler of all places.... if high, ignored. probbly cradle detect (low if cradled)
		5		IN		0
		6		OUT	LO	1			MMC clock
		7		IN		0
		8		OUT	LO	1			MMC CS0
		9		IN		0
		10		IN		0			sd card detect (Active low)
		11		IN		0
		12		IN		0	RE FE 	headphone detect?		[GUESS]
		13		IN		0
		14		IN		0
		15		OUT	LO	2			nCS1
		16		OUT	LO	2			PWM0
		17		OUT	HI	0
		18		OUT	LO	0
		19		IN		0
		20		IN		0
		21		IN		0
		22		IN		0
		23		OUT	LO	0
		24		OUT	LO	0
		25		IN		0
		26		OUT	HI	0			WM9712L power? enabled when enabling pen. hi for power
		27		OUT	LO	0
		28		IN		1			AC97 bit_clk
		29		IN		1			AC97 Sdata_in0
		30		OUT	LO	2			AC97 Sdata_out
		31		OUT	LO	2			AC97 sync
		32		OUT	HI	0
		33		OUT	LO	0
		34		IN		1			FFUART RX
		35		IN		0
		36		OUT	LO	0
		37		OUT	HI	0
		38		OUT	LO	0
		39		OUT	LO	2			FFUART TX
		40		IN		0	FE 		hotsync button?		[GUESS]
		41		OUT	LO	0
		42		IN		3			HWUART RX
		43		OUT	LO	3			HWUART TX
		44		IN		3			HWUART CTS
		45		OUT	LO	3			HWUART RTS
		46		IN		2			STUART RX
		47		OUT	LO	1			STUART TX
		48		OUT	LO	0
		49		IN		0
		50		IN		0	RE 		probably pen detect
		51		IN		0			card write protect switch (active high)
		52		IN		0			low on boot causes manufacturing test to be entered
		53		IN		0
		54		OUT	LO	0
		55		OUT	LO	0			SD power (active high)
		56		OUT	HI	0
		57		OUT	HI	0
		58		OUT	LO	2			LDD[0]
		59		OUT	LO	2			LDD[1]
		60		OUT	LO	2			LDD[2]
		61		OUT	LO	2			LDD[3]
		62		OUT	LO	2			LDD[4]
		63		OUT	LO	2			LDD[5]
		64		OUT	LO	2			LDD[6]
		65		OUT	LO	2			LDD[7]
		66		OUT	LO	2			LDD[8]
		67		OUT	LO	2			LDD[9]
		68		OUT	LO	2			LDD[10]
		69		OUT	LO	2			LDD[11]
		70		OUT	LO	2			LDD[12]
		71		OUT	LO	2			LDD[13]
		72		OUT	LO	2			LDD[14]
		73		OUT	LO	2			LDD[15]
		74		OUT	LO	2			LCD FCLK
		75		OUT	LO	2			LCD LCLK
		76		OUT	LO	2			LCD PCLK
		77		OUT	LO	2			LCD AC BIAS
		78		OUT	LO	0
		79		IN		0			NAND nRDY
		80		OUT	LO	0
		81		OUT	LO	0
		82		OUT	LO	0
		83		OUT	LO	0
		84		OUT	LO	0
		85		IN		0
*/
// clang-format on

namespace {
    class DeviceTungstenE2 : public Device {
       public:
        DeviceTungstenE2(enum DeviceType5 type, struct SocPeriphs *sp,
                         struct Reschedule reschedule, struct GetEmuTime getTime,
                         struct Keypad *kp, uint8_t *nandContent, size_t nandSize,
                         const struct MemoryBuffer *pageBuffer);

        void Periodic(uint32_t tier) override {
            if (tier == DEVICE_PERIODIC_TIER0) directNandPeriodic(nand);
        }

        void PcmPeriodic() override { wm9712Lperiodic(wm9712L); }

        void Touch(int x, int y) override {
            wm9712LsetPen(wm9712L, (x >= 0 && y >= 0) ? 280 + 10 * x : -1,
                          (x >= 0 && y >= 0) ? 210 + 7 * y : -1, 1000);
        }

        bool TaskRequired(uint32_t tier) override {
            return tier == DEVICE_PERIODIC_TIER0 ? directNandTaskRequired(nand) : true;
        }

        void SetAudioQueue(struct AudioQueue *audioQueue) override {
            wm9712LsetAudioQueue(wm9712L, audioQueue);
        }

        void SetSdCard(struct VSD *vsd) override { pxaGpioSetState(gpio, 10, !vsd); }

        void Save(Savestate<ChunkType> &savestate) override { DoSave(savestate); }
        void Save(SavestateProbe<ChunkType> &savestate) override { DoSave(savestate); }

        void Load(SavestateLoader<ChunkType> &loader) override {
            wm9712Load(wm9712L, loader);
            directNandLoad(nand, loader);
            bcm2035Load(bcm2035, loader);
        }

       private:
        template <typename T>
        void DoSave(T &savestate) {
            wm9712Lsave(wm9712L, savestate);
            directNandSave(nand, savestate);
            bcm2035Save(bcm2035, savestate);
        }

        static void PrvReschedule(void *ctx, uint32_t task);

       private:
        struct WM9712L *wm9712L;
        struct DirectNAND *nand;
        struct Bcm2035 *bcm2035;
        struct PxaGpio *gpio;

        struct Reschedule reschedule;
    };

    void DeviceTungstenE2::PrvReschedule(void *ctx, uint32_t task) {
        auto *dev = reinterpret_cast<DeviceTungstenE2 *>(ctx);

        if (task == RESCHEDULE_TASK_NAND)
            dev->reschedule.rescheduleCb(dev->reschedule.ctx, RESCHEDULE_TASK_DEVICE_TIER0);
    }

    DeviceTungstenE2::DeviceTungstenE2(enum DeviceType5 type, struct SocPeriphs *sp,
                                       struct Reschedule reschedule, struct GetEmuTime getTime,
                                       struct Keypad *kp, uint8_t *nandContent, size_t nandSize,
                                       const struct MemoryBuffer *pageBuffer)
        : Device(type), gpio(sp->gpio), reschedule(reschedule) {
        static const struct NandSpecs nandSpecs = {
            .bytesPerPage = 528,
            .blocksPerDevice = 2048,
            .pagesPerBlockLg2 = 5,
            .flags = NAND_FLAG_SAMSUNG_ADDRESSED_VIA_AREAS,
            .devIdLen = 2,
            .devId = {0xec, 0x75},
        };

        struct Reschedule deviceReschedule = {.rescheduleCb = PrvReschedule, .ctx = this};

        wm9712L = wm9712LInit(sp->ac97, sp->gpio, 50);
        if (!wm9712L) ERR("Cannot init WM9712L");

        bcm2035 = bcm2035Init(getTime);

        nand = directNandInit(sp->mem, deviceReschedule, 0x04000002UL, 0x04000004UL,
                              0x04000000UL, 0x00fffff9ul, sp->gpio, 79, &nandSpecs, nandContent,
                              nandSize, pageBuffer);
        if (!nand) ERR("Cannot init NAND");

        if (!keypadAddGpioKey(kp, keyIdHard1, 11, false))
            ERR("Cannot init hardkey1 (datebook)\n");

        if (!keypadAddGpioKey(kp, keyIdHard2, 13, false))
            ERR("Cannot init hardkey2 (address)\n");

        if (!keypadAddGpioKey(kp, keyIdHard3, 7, false)) ERR("Cannot init hardkey3 (ToDo)\n");

        if (!keypadAddGpioKey(kp, keyIdHard4, 5, false)) ERR("Cannot init hardkey4 (Memos)\n");

        if (!keypadAddGpioKey(kp, keyIdDown, 21, false)) ERR("Cannot init down key\n");

        if (!keypadAddGpioKey(kp, keyIdUp, 22, false)) ERR("Cannot init up key\n");

        if (!keypadAddGpioKey(kp, keyIdLeft, 19, false)) ERR("Cannot init left key\n");

        if (!keypadAddGpioKey(kp, keyIdRight, 20, false)) ERR("Cannot init right key\n");

        if (!keypadAddGpioKey(kp, keyIdSelect, 14, false)) ERR("Cannot init select key\n");

        // XXX: change this to true to boot with power key inverted. press ESC to release.
        if (!keypadAddGpioKey(kp, keyIdPower, 2, false)) ERR("Cannot init power key\n");

        pxaGpioSetState(sp->gpio, 1, true);  // reset button

        // not in cradle (also prevents FIQ wtchdog from firing after 5 sec)
        pxaGpioSetState(sp->gpio, 4, true);

        pxaGpioSetState(sp->gpio, 40, true);  // no hotsync button pressed [??]

        pxaGpioSetState(sp->gpio, 12, false);  // no headphones [??]

        pxaGpioSetState(sp->gpio, 52, true);  // no manufacturing test mode please

        wm9712LsetAuxVoltage(wm9712L, WM9712LauxPinBmon, 4200 / 3);  // main battery is 4.2V

        SetSdCard(nullptr);

        // yes, this is really swapped
        sp->dbgUart = sp->uarts[3];  // BTUART
        sp->btUart = sp->uarts[1];   // HWUART

        sp->nand = directNandGetNand(nand);
        bcm2035RegisterWithUart(bcm2035, sp->btUart);
    }
}  // namespace

struct Device *deviceSetupTungstenE2(enum DeviceType5 type, struct SocPeriphs *sp,
                                     struct Reschedule reschedule, struct GetEmuTime getEmuTime,
                                     struct Keypad *kp, struct VSD *vsd, uint8_t *nandContent,
                                     size_t nandSize, const struct MemoryBuffer *nandPageBuffer) {
    return new DeviceTungstenE2(type, sp, reschedule, getEmuTime, kp, nandContent, nandSize,
                                nandPageBuffer);
}
