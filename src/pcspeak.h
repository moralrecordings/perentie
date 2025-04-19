#include <stdint.h>
#include <stdlib.h>

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

pt_pc_speaker_data* create_pc_speaker_data(uint16_t* data, size_t data_len, uint16_t playback_freq, char* name);
void destroy_pc_speaker_data(pt_pc_speaker_data* spk);
pt_pc_speaker_ifs* load_pc_speaker_ifs(const char* path);
