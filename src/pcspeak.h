#include <stdint.h>
#include <stdlib.h>

typedef uint8_t byte;

typedef struct pt_pc_speaker_data pt_pc_speaker_data;
typedef struct pt_pc_speaker_ifs pt_pc_speaker_ifs;

struct pt_pc_speaker_data {
    uint16_t* data;
    size_t data_len;
    uint16_t playback_freq;
    char* name;
};

struct pt_pc_speaker_ifs {
    pt_pc_speaker_data** sounds;
    size_t sounds_len;
};

void pcspeaker_init();
void pcspeaker_shutdown();
void pcspeaker_tone_raw(uint16_t value);
void pcspeaker_tone(float freq);
void pcspeaker_play_sample(byte* data, size_t len, int rate);
void pcspeaker_play_data(uint16_t* data, size_t len, int rate);
void pcspeaker_sample_update();
void pcspeaker_data_update();
void pcspeaker_stop();

pt_pc_speaker_data* create_pc_speaker_data(uint16_t* data, size_t data_len, uint16_t playback_freq, char* name);
void destroy_pc_speaker_data(pt_pc_speaker_data* spk);
pt_pc_speaker_ifs* load_pc_speaker_ifs(const char* path);
