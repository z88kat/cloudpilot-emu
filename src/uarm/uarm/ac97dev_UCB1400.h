//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _AC97_UCB1400_H_
#define _AC97_UCB1400_H_

#include <cstdint>

#include "pxa_AC97.h"
#include "pxa_GPIO.h"

struct UCB1400;
struct AudioQueue;

struct UCB1400 *ucb1400Init(struct PxaAC97 *ac97, struct PxaGpio *gpio, int8_t irqPin);
void ucb1400periodic(struct UCB1400 *ucb);

void ucb1400setAuxVoltage(struct UCB1400 *ucb, uint8_t adcIdx, uint32_t mV);
void ucb1400setPen(struct UCB1400 *ucb, int16_t x, int16_t y);  // raw ADC values, negative for up

void ucb1400setGpioInputVal(struct UCB1400 *ucb, uint8_t gpioIdx, bool high);
bool ucb1400getGpioOutputVal(struct UCB1400 *ucb, uint8_t gpioIdx);

void ucb1400setAudioQueue(struct UCB1400 *ucb, struct AudioQueue *audioQueue);

template <typename T>
void ucb1400Save(struct UCB1400 *ucb, T &savestate);

template <typename T>
void ucb1400Load(struct UCB1400 *ucb, T &loader);

#endif
