#ifndef PERENTIE_MUSICRAD_H
#define PERENTIE_MUSICRAD_H

#include <stdint.h>

enum pt_musicrad_cmd_type {
    MUSICRAD_CMD_NULL,
    MUSICRAD_CMD_PLAY,
    MUSICRAD_CMD_STOP,
    MUSICRAD_CMD_SET_MASTER_VOLUME,
    MUSICRAD_CMD_SET_POSITION,
    MUSICRAD_CMD_FADE_IN,
    MUSICRAD_CMD_FADE_OUT,
};

typedef union {
    enum pt_musicrad_cmd_type type;

    struct {
        enum pt_musicrad_cmd_type type;
        uint8_t vol;
    } volume;

    struct {
        enum pt_musicrad_cmd_type type;
        uint8_t order;
        uint8_t line;
    } position;

    struct {
        enum pt_musicrad_cmd_type type;
        uint32_t duration;
    } fade;
} pt_musicrad_cmd;

typedef struct RADPlayer RADPlayer;
void radplayer_init();
void radplayer_shutdown();
bool radplayer_load_file(const char* path);
void radplayer_update();
void radplayer_play();
void radplayer_stop();
char* radplayer_get_path();
void radplayer_set_master_volume(uint8_t vol);
uint8_t radplayer_get_master_volume();
void radplayer_set_position(uint8_t order, uint8_t line);
uint8_t radplayer_get_order();
uint8_t radplayer_get_line();
void radplayer_fade_in(uint32_t duration);
void radplayer_fade_out(uint32_t duration);

void rad_load(RADPlayer* rad);
void rad_play(RADPlayer* rad);
void rad_stop(RADPlayer* rad);
bool rad_update(RADPlayer* rad);
int rad_get_hertz(RADPlayer* rad);
int rad_get_play_time_in_seconds(RADPlayer* rad);
int rad_get_tune_pos(RADPlayer* rad);
int rad_get_tune_length(RADPlayer* rad);
int rad_get_tune_line(RADPlayer* rad);
void rad_set_master_volume(RADPlayer* rad, uint8_t vol);
uint8_t rad_get_master_volume(RADPlayer* rad);
uint8_t rad_get_speed(RADPlayer* rad);
void rad_set_position(RADPlayer* rad, uint8_t order, uint8_t line);
uint8_t rad_get_order(RADPlayer* rad);
uint8_t rad_get_line(RADPlayer* rad);
void rad_fade_in(RADPlayer* rad, uint32_t duration);
void rad_fade_out(RADPlayer* rad, uint32_t duration);

#if RAD_DETECT_REPEATS
uint32_t rad_compute_total_time(RADPlayer* rad);
#endif

#endif
