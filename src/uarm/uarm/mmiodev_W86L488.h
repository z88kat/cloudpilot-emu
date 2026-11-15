//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _W86L488_H_
#define _W86L488_H_

#include <cstdint>
#include "pxa_GPIO.h"
#include "mem.h"
#include "vSD.h"

#define W86L488_BASE_T3 	0x08000000ul
#define W86L488_BASE_AXIM	0x0c000000ul

struct W86L488;


struct W86L488* w86l488init(struct ArmMem *physMem, struct PxaGpio *gpio, uint32_t base, struct VSD *card, int intPin /* negative for none */);

//inputs only
void w86l488gpioSetVal(struct W86L488* wl, unsigned gpioNum, bool hi);

template <typename T>
void w86l488Save(struct W86L488 *wl, T &savestate);

template <typename T>
void w86l488Load(struct W86L488 *wl, T &loader);


#endif
