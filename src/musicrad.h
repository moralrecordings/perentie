#ifndef PERENTIE_MUSICRAD_H
#define PERENTIE_MUSICRAD_H

#include <stdint.h>

typedef struct RADPlayer RADPlayer;
void radplayer_init();
void radplayer_shutdown();
bool radplayer_load_file(const char* path);
void radplayer_update();
void radplayer_play();
void radplayer_stop();
char* radplayer_get_path();
void radplayer_set_master_volume(int vol);
int radplayer_get_master_volume();
void radplayer_set_position(int order, int line);
int radplayer_get_order();
int radplayer_get_line();

void rad_load(RADPlayer* rad);
void rad_play(RADPlayer* rad);
void rad_stop(RADPlayer* rad);
bool rad_update(RADPlayer* rad);
int rad_get_hertz(RADPlayer* rad);
int rad_get_play_time_in_seconds(RADPlayer* rad);
int rad_get_tune_pos(RADPlayer* rad);
int rad_get_tune_length(RADPlayer* rad);
int rad_get_tune_line(RADPlayer* rad);
void rad_set_master_volume(RADPlayer* rad, int vol);
int rad_get_master_volume(RADPlayer* rad);
int rad_get_speed(RADPlayer* rad);
void rad_set_position(RADPlayer* rad, int order, int line);
int rad_get_order(RADPlayer* rad);
int rad_get_line(RADPlayer* rad);

#if RAD_DETECT_REPEATS
uint32_t rad_compute_total_time(RADPlayer* rad);
#endif

#endif
