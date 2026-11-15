//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _TPS65010_H_
#define _TPS65010_H_

#include <cstdint>

#include "pxa_I2C.h"

struct Tps65010;

struct Tps65010* tps65010Init(struct PxaI2c* i2c);

template <typename T>
void tps65010Save(struct Tps65010 *tps, T &savestate);

template <typename T>
void tps65010Load(struct Tps65010 *tps, T &loader);

#endif
