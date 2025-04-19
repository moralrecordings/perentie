#include <stdio.h>

#include "log.h"
#include "pcspeak.h"
#include "utils.h"

pt_pc_speaker_data* create_pc_speaker_data(uint16_t* data, size_t data_len, uint16_t playback_freq, char* name)
{
    pt_pc_speaker_data* spk = (pt_pc_speaker_data*)calloc(1, sizeof(pt_pc_speaker_data));
    spk->data = data;
    spk->data_len = data_len;
    spk->playback_freq = playback_freq;
    spk->name = name;
    return spk;
}

void destroy_pc_speaker_data(pt_pc_speaker_data* spk)
{
    if (!spk)
        return;
    free(spk->data);
    if (spk->name)
        free(spk->name);
    free(spk);
}

// Info: https://moddingwiki.shikadi.net/wiki/Inverse_Frequency_Sound_format

pt_pc_speaker_ifs* load_pc_speaker_ifs(const char* path)
{
    FILE* fp;
    fp = fopen(path, "rb");
    if (!fp) {
        log_print("load_pc_speaker_ifs: could not open file %s\n", path);
        return NULL;
    }

    uint32_t magic = fread_u32be(fp);
    if (magic != 0x534e4400) { // b"SND\x00"
        log_print("load_pc_speaker_ifs: file %s is not an Inverse Frequency Sound file\n", path);
        fclose(fp);
        return NULL;
    }

    // the header can't be trusted, infer the data from offsets
    fseek(fp, 0, SEEK_END);
    size_t file_end = ftell(fp);
    fseek(fp, 16, SEEK_SET);
    size_t header_end = fread_u16le(fp);
    fseek(fp, 16, SEEK_SET);
    uint16_t sfx_count = (header_end - 16) >> 4;

    pt_pc_speaker_ifs* result = (pt_pc_speaker_ifs*)calloc(1, sizeof(pt_pc_speaker_ifs));
    result->sounds = (pt_pc_speaker_data**)calloc(sfx_count, sizeof(pt_pc_speaker_data*));
    for (int i = 0; i < sfx_count; i++) {
        fseek(fp, 16 + i * 16, SEEK_SET);
        size_t sound_offset = fread_u16le(fp);
        fread_u8(fp);
        fread_u8(fp);
        char* name = (char*)calloc(16, sizeof(char));
        fread(name, sizeof(char), 12, fp);
        size_t sound_end = ftell(fp) == header_end ? file_end : fread_u16le(fp);
        fseek(fp, sound_offset, SEEK_SET);
        size_t data_len = (sound_end - sound_offset) >> 1;
        uint16_t* data = (uint16_t*)malloc(sizeof(uint16_t) * data_len);
        fread(data, sizeof(uint16_t), data_len, fp);
        result->sounds[i] = create_pc_speaker_data(data, data_len, 140, name);
        log_print("load_pc_speaker_ifs: %d - name: %s, len: %d\n", i, name, data_len);
    }
    result->sounds_len = sfx_count;

    fclose(fp);
    return result;
}
