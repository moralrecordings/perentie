#ifndef PERENTIE_MUSICRAD_H
#define PERENTIE_MUSICRAD_H

#include <stdint.h>

typedef struct RADPlayer RADPlayer;
void rad_init(RADPlayer* rad, const void* tune, void (*opl3)(void*, uint16_t, uint8_t), void* arg);
void rad_stop(RADPlayer* rad);
bool rad_update(RADPlayer* Rad);
int rad_get_hertz(RADPlayer* rad);
int rad_get_play_time_in_seconds(RADPlayer* rad);
int rad_get_tune_pos(RADPlayer* rad);
int rad_get_tune_length(RADPlayer* rad);
int rad_get_tune_line(RADPlayer* rad);
void rad_set_master_volume(RADPlayer* rad, int vol);
int rad_get_master_volume(RADPlayer* rad);
int rad_get_speed(RADPlayer* rad);

#if RAD_DETECT_REPEATS
uint32_t rad_compute_total_time(RADPlayer* rad);
#endif

#endif
