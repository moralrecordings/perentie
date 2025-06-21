/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

// Rough C port of the DOSBox PC speaker emulation code

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

// CycleMax = 3000
// Cycles per millisecond

// static inline float PIC_TickIndex(void) {
//	return (CPU_CycleMax-CPU_CycleLeft-CPU_Cycles)/(float)CPU_CycleMax;
// }

#ifndef PI
#define PI 3.14159265358979323846
#endif

#define PIT_TICK_RATE 1193182

#define SPKR_ENTRIES 1024
#define SPKR_VOLUME 5000
// #define SPKR_SHIFT 8
#define SPKR_SPEED (float)((SPKR_VOLUME * 2) / 0.070f)

enum SPKR_MODES { SPKR_OFF, SPKR_ON, SPKR_PIT_OFF, SPKR_PIT_ON };

typedef struct {
    float index;
    float vol;
} DelayEntry;

static struct {
    enum SPKR_MODES mode;
    uint32_t pit_mode;
    uint32_t rate;

    float pit_last;
    float pit_new_max, pit_new_half;
    float pit_max, pit_half;
    float pit_index;
    float volwant, volcur;
    uint32_t last_ticks; // only used to check the time for turning off the sound, not used for sampling
    float last_index;
    uint32_t min_tr;
    DelayEntry entries[SPKR_ENTRIES];
    uint32_t used;
    uint32_t (*get_ticks)();
} spkr;

static void AddDelayEntry(float index, float vol)
{
    if (spkr.used == SPKR_ENTRIES) {
        return;
    }
    spkr.entries[spkr.used].index = index;
    spkr.entries[spkr.used].vol = vol;
    spkr.used++;
}

static void ForwardPIT(float newindex)
{
    float passed = (newindex - spkr.last_index);
    float delay_base = spkr.last_index;
    spkr.last_index = newindex;
    switch (spkr.pit_mode) {
    case 0:
        return;
    case 1:
        return;
    case 2:
        while (passed > 0) {
            /* passed the initial low cycle? */
            if (spkr.pit_index >= spkr.pit_half) {
                /* Start a new low cycle */
                if ((spkr.pit_index + passed) >= spkr.pit_max) {
                    float delay = spkr.pit_max - spkr.pit_index;
                    delay_base += delay;
                    passed -= delay;
                    spkr.pit_last = -SPKR_VOLUME;
                    if (spkr.mode == SPKR_PIT_ON)
                        AddDelayEntry(delay_base, spkr.pit_last);
                    spkr.pit_index = 0;
                } else {
                    spkr.pit_index += passed;
                    return;
                }
            } else {
                if ((spkr.pit_index + passed) >= spkr.pit_half) {
                    float delay = spkr.pit_half - spkr.pit_index;
                    delay_base += delay;
                    passed -= delay;
                    spkr.pit_last = SPKR_VOLUME;
                    if (spkr.mode == SPKR_PIT_ON)
                        AddDelayEntry(delay_base, spkr.pit_last);
                    spkr.pit_index = spkr.pit_half;
                } else {
                    spkr.pit_index += passed;
                    return;
                }
            }
        }
        break;
        // END CASE 2
    case 3:
        while (passed > 0) {
            /* Determine where in the wave we're located */
            if (spkr.pit_index >= spkr.pit_half) {
                if ((spkr.pit_index + passed) >= spkr.pit_max) {
                    float delay = spkr.pit_max - spkr.pit_index;
                    delay_base += delay;
                    passed -= delay;
                    spkr.pit_last = SPKR_VOLUME;
                    if (spkr.mode == SPKR_PIT_ON)
                        AddDelayEntry(delay_base, spkr.pit_last);
                    spkr.pit_index = 0;
                    /* Load the new count */
                    spkr.pit_half = spkr.pit_new_half;
                    spkr.pit_max = spkr.pit_new_max;
                } else {
                    spkr.pit_index += passed;
                    return;
                }
            } else {
                if ((spkr.pit_index + passed) >= spkr.pit_half) {
                    float delay = spkr.pit_half - spkr.pit_index;
                    delay_base += delay;
                    passed -= delay;
                    spkr.pit_last = -SPKR_VOLUME;
                    if (spkr.mode == SPKR_PIT_ON)
                        AddDelayEntry(delay_base, spkr.pit_last);
                    spkr.pit_index = spkr.pit_half;
                    /* Load the new count */
                    spkr.pit_half = spkr.pit_new_half;
                    spkr.pit_max = spkr.pit_new_max;
                } else {
                    spkr.pit_index += passed;
                    return;
                }
            }
        }
        break;
        // END CASE 3
    case 4:
        if (spkr.pit_index < spkr.pit_max) {
            /* Check if we're gonna pass the end this block */
            if (spkr.pit_index + passed >= spkr.pit_max) {
                float delay = spkr.pit_max - spkr.pit_index;
                delay_base += delay;
                passed -= delay;
                spkr.pit_last = -SPKR_VOLUME;
                if (spkr.mode == SPKR_PIT_ON)
                    AddDelayEntry(delay_base, spkr.pit_last); // No new events unless reprogrammed
                spkr.pit_index = spkr.pit_max;
            } else
                spkr.pit_index += passed;
        }
        break;
        // END CASE 4
    }
}

void PCSPEAKER_SetCounter(uint32_t cntr, uint32_t mode, float tick_index)
{
    if (!spkr.last_ticks) {
        // if(spkr.chan) spkr.chan->Enable(true);
        spkr.last_index = 0;
    }
    spkr.last_ticks = spkr.get_ticks();
    ForwardPIT(tick_index);
    switch (mode) {
    case 0: /* Mode 0 one shot, used with realsound */
        if (spkr.mode != SPKR_PIT_ON)
            return;
        if (cntr > 80) {
            cntr = 80;
        }
        spkr.pit_last = ((float)cntr - 40) * (SPKR_VOLUME / 40.0f);
        AddDelayEntry(tick_index, spkr.pit_last);
        spkr.pit_index = 0;
        break;
    case 1:
        if (spkr.mode != SPKR_PIT_ON)
            return;
        spkr.pit_last = SPKR_VOLUME;
        AddDelayEntry(tick_index, spkr.pit_last);
        break;
    case 2: /* Single cycle low, rest low high generator */
        spkr.pit_index = 0;
        spkr.pit_last = -SPKR_VOLUME;
        AddDelayEntry(tick_index, spkr.pit_last);
        spkr.pit_half = (1000.0f / PIT_TICK_RATE) * 1;
        spkr.pit_max = (1000.0f / PIT_TICK_RATE) * cntr;
        break;
    case 3: /* Square wave generator */
        if (cntr == 0 || cntr < spkr.min_tr) {
            /* skip frequencies that can't be represented */
            spkr.pit_last = 0;
            spkr.pit_mode = 0;
            return;
        }
        spkr.pit_new_max = (1000.0f / PIT_TICK_RATE) * cntr;
        spkr.pit_new_half = spkr.pit_new_max / 2;
        break;
    case 4: /* Software triggered strobe */
        spkr.pit_last = SPKR_VOLUME;
        AddDelayEntry(tick_index, spkr.pit_last);
        spkr.pit_index = 0;
        spkr.pit_max = (1000.0f / PIT_TICK_RATE) * cntr;
        break;
    default:
        // unhandled speaker mode
        return;
    }
    spkr.pit_mode = mode;
}

void PCSPEAKER_SetType(uint32_t mode, float tick_index)
{
    if (!spkr.last_ticks) {
        // if(spkr.chan) spkr.chan->Enable(true);
        spkr.last_index = 0;
    }
    spkr.last_ticks = spkr.get_ticks();
    ForwardPIT(tick_index);
    switch (mode) {
    case 0:
        spkr.mode = SPKR_OFF;
        AddDelayEntry(tick_index, -SPKR_VOLUME);
        break;
    case 1:
        spkr.mode = SPKR_PIT_OFF;
        AddDelayEntry(tick_index, -SPKR_VOLUME);
        break;
    case 2:
        spkr.mode = SPKR_ON;
        AddDelayEntry(tick_index, SPKR_VOLUME);
        break;
    case 3:
        if (spkr.mode != SPKR_PIT_ON) {
            AddDelayEntry(tick_index, spkr.pit_last);
        }
        spkr.mode = SPKR_PIT_ON;
        break;
    };
}

void PCSPEAKER_CallBack(int16_t* sndptr, intptr_t len)
{
    ForwardPIT(1);
    spkr.last_index = 0;
    uint32_t count = len;
    uint32_t pos = 0;
    float sample_base = 0;
    float sample_add = (1.0001f) / len;
    while (count--) {
        float index = sample_base;
        sample_base += sample_add;
        float end = sample_base;
        double value = 0;
        while (index < end) {
            /* Check if there is an upcoming event */
            if (spkr.used && spkr.entries[pos].index <= index) {
                spkr.volwant = spkr.entries[pos].vol;
                pos++;
                spkr.used--;
                continue;
            }
            float vol_end;
            if (spkr.used && spkr.entries[pos].index < end) {
                vol_end = spkr.entries[pos].index;
            } else
                vol_end = end;
            float vol_len = vol_end - index;
            /* Check if we have to slide the volume */
            float vol_diff = spkr.volwant - spkr.volcur;
            if (vol_diff == 0) {
                value += spkr.volcur * vol_len;
                index += vol_len;
            } else {
                /* Check how long it will take to goto new level */
                float vol_time = fabsf(vol_diff) / SPKR_SPEED;
                if (vol_time <= vol_len) {
                    /* Volume reaches endpoint in this block, calc until that point */
                    value += vol_time * spkr.volcur;
                    value += vol_time * vol_diff / 2;
                    index += vol_time;
                    spkr.volcur = spkr.volwant;
                } else {
                    /* Volume still not reached in this block */
                    value += spkr.volcur * vol_len;
                    if (vol_diff < 0) {
                        value -= (SPKR_SPEED * vol_len * vol_len) / 2;
                        spkr.volcur -= SPKR_SPEED * vol_len;
                    } else {
                        value += (SPKR_SPEED * vol_len * vol_len) / 2;
                        spkr.volcur += SPKR_SPEED * vol_len;
                    }
                    index += vol_len;
                }
            }
        }
        *sndptr++ = (int16_t)(value / sample_add);
    }

    // Turn off speaker after 10 seconds of idle or one second idle when in off mode
    bool turnoff = false;
    uint32_t test_ticks = spkr.get_ticks();
    if ((spkr.last_ticks + 10000) < test_ticks)
        turnoff = true;
    if ((spkr.mode == SPKR_OFF) && ((spkr.last_ticks + 1000) < test_ticks))
        turnoff = true;

    if (turnoff) {
        if (spkr.volwant == 0) {
            spkr.last_ticks = 0;
            // if(spkr.chan) spkr.chan->Enable(false);
        } else {
            if (spkr.volwant > 0)
                spkr.volwant--;
            else
                spkr.volwant++;
        }
    }
}

void PCSPEAKER_ShutDown()
{
}

void PCSPEAKER_Init(uint32_t rate, uint32_t (*get_ticks)())
{
    spkr.mode = SPKR_OFF;
    spkr.last_ticks = 0;
    spkr.last_index = 0;
    spkr.rate = rate;
    spkr.pit_mode = 3;
    spkr.pit_max = (1000.0f / PIT_TICK_RATE) * 1320;
    spkr.pit_half = spkr.pit_max / 2;
    spkr.pit_new_max = spkr.pit_max;
    spkr.pit_new_half = spkr.pit_half;
    spkr.pit_index = 0;
    spkr.min_tr = (PIT_TICK_RATE + spkr.rate / 2 - 1) / (spkr.rate / 2);
    spkr.used = 0;
    spkr.volwant = 0;
    spkr.get_ticks = get_ticks;
    /* Register the sound channel */
    // spkr.chan=MixerChan.Install(&PCSPEAKER_CallBack,spkr.rate,"SPKR");
}
