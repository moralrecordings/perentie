#ifndef WOODYPC_PCSPEAK_H
#define WOODYPC_PCSPEAK_H

#include <stdint.h>

void PCSPEAKER_SetCounter(uint32_t cntr, uint32_t mode, float tick_index);
void PCSPEAKER_SetType(uint32_t mode, float tick_index);
void PCSPEAKER_CallBack(int16_t* sndptr, intptr_t len);

void PCSPEAKER_ShutDown();
void PCSPEAKER_Init(uint32_t rate, uint32_t (*get_ticks)());

#endif
