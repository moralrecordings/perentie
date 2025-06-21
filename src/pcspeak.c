#include <stdio.h>
#include <string.h>

#ifdef SYSTEM_DOS
#include "dos.h"
#include <dpmi.h>
#endif

#include "log.h"
#include "pcspeak.h"
#include "system.h"
#include "utils.h"

#define TIMER_PIT_CLOCK 1193180L
#define TIMER_HZ 1000
#define TIMER_HIRES_HZ 16000
#define TIMER_HIRES_DIV 16

#define PCSPEAKER_OFF 0
#define PCSPEAKER_TONE 1
#define PCSPEAKER_SAMPLE_8K 2
#define PCSPEAKER_SAMPLE_16K 3
#define PCSPEAKER_DATA 4
struct pcspeaker_data_t {
    uint8_t mode;
    byte* sample_data;
    size_t sample_len;
    uint16_t* data;
    size_t data_count;
    uint16_t data_rate;
    uint16_t data_ticks;
    size_t offset;
};
static struct pcspeaker_data_t _pcspeaker = { 0 };
static uint8_t pcspeaker_lut[256] = { 0 };
void pcspeaker_sample_update();
void pcspeaker_data_update();
void pcspeaker_play_sample(byte* data, size_t len, int rate);
void pcspeaker_play_data(uint16_t* data, size_t len, int rate);

void pcspeaker_stop();
void pcspeaker_init()
{
    // 6-bit LUT for the sample playback mode
    for (int i = 0; i < 256; i++) {
        pcspeaker_lut[i] = (uint8_t)(i * TIMER_PIT_CLOCK / (TIMER_HIRES_HZ * 255));
    }
    memset(&_pcspeaker, 0, sizeof(_pcspeaker));
#ifdef SYSTEM_DOS
    LOCK_DATA(_pcspeaker);
    // Real hardware might start in a cooked state, reset just to be sure.
    // On emulated hardware all this does is create an ugly pop sound.
    pcspeaker_stop();
#endif
}

void pcspeaker_shutdown()
{
    pcspeaker_stop();
}

void pcspeaker_tone_raw(uint16_t value)
{
    // Load tone frequency into PIT channel 2 data port
    // The PC speaker is attached to PIT channel 2 and has two states: high
    // and low. The square wave generator will cycle between high and low
    // after a specified number of ticks of the main clock (1193180Hz),
    // making a tone frequency at the loudest possible volume.

    // PIT mode 3 (square wave generator)
    pt_sys.beep->set_mode(3, true);
    pt_sys.beep->set_counter_16(value);
    pt_sys.beep->set_gate(true);
}

void pcspeaker_tone(float freq)
{
    _pcspeaker.mode = PCSPEAKER_TONE;
    uint16_t units = TIMER_PIT_CLOCK / freq;
    pcspeaker_tone_raw(units);
}

void pcspeaker_play_sample(byte* data, size_t len, int rate)
{
    uint8_t mode = 0;
    if (!pt_sys.timer->supports_hires()) {
        log_print(
            "pcspeaker_play_sample: High-resolution timer missing (DOS inside Windows?), sample playback disabled\n");
        return;
    } else if (rate == 8000) {
        mode = PCSPEAKER_SAMPLE_8K;
    } else if (rate == 16000) {
        mode = PCSPEAKER_SAMPLE_16K;
    } else {
        log_print("pcspeaker_play_sample: audio must have a sample rate of 8000 or 16000\n");
        return;
    }
    pcspeaker_stop();
    _pcspeaker.sample_data = data;
    _pcspeaker.sample_len = len;
    _pcspeaker.offset = 0;
    _pcspeaker.mode = mode;
#ifdef SYSTEM_DOS
    _go32_dpmi_lock_data(data, len);
#endif
    pt_sys.timer->set_hires(true);
    // PIT mode/command register:
    // - counter 2 (PC speaker)
    // - bits 0-7 only
    // - mode 0 (interrupt on terminal count)
    pt_sys.beep->set_mode(0, false);
    pt_sys.beep->set_gate(true);
}

void pcspeaker_play_data(uint16_t* data, size_t len, int rate)
{
    if ((rate > 1000) || (rate <= 0)) {
        log_print("pcspeaker_play_data: playback rate must be between 1 and 1000Hz");
        return;
    }
    pcspeaker_stop();
    _pcspeaker.mode = PCSPEAKER_DATA;
#ifdef SYSTEM_DOS
    _go32_dpmi_lock_data(data, len * sizeof(uint16_t));
#endif
    _pcspeaker.data = data;
    _pcspeaker.data_count = len;
    _pcspeaker.data_rate = TIMER_HIRES_HZ / rate;
    _pcspeaker.data_ticks = 0;
}

void pcspeaker_sample_update()
{
    // This is the same trick used by Access Software's "RealSound".
    // Even though we can only send an on-off signal to the PC speaker,
    // the speaker cone is still subject to real world physics.
    // If we change the signal faster than the cone can move, then
    // it will average out to an analogue waveform.

    // We set PIT channel 2 to mode 0; this means whenever we write [value]
    // to counter 2, the PC speaker output will go high for [value] main
    // clock ticks, then go low.

    // ------------0x4a------------
    // --- value ----
    // _____________
    //              |______________ -> PC speaker

    // Say that we ran our main timer at 16000Hz. That gives us 0x4a ticks
    // of the main clock between each timer interrupt. So if we had an
    // 8000Hz unsigned 8-bit WAV, we could convert each sample to the 0x00-0x4a
    // range, and make our main timer interrupt set counter 2 (twice per sample)
    // to play it back.
    size_t offset = 0;
    if (_pcspeaker.mode == PCSPEAKER_SAMPLE_8K) {
        offset = _pcspeaker.offset >> 1;
    } else if (_pcspeaker.mode == PCSPEAKER_SAMPLE_16K) {
        offset = _pcspeaker.offset;
    } else {
        return;
    }
    if (offset >= _pcspeaker.sample_len) {
        pcspeaker_stop();
    } else {
        // PIT channel 2 data port
        // outportb(0x42, _pcspeaker.sample_data[offset]);
        pt_sys.beep->set_counter_8(pcspeaker_lut[_pcspeaker.sample_data[offset]]);
        _pcspeaker.offset += 1;
    }
}

void pcspeaker_data_update()
{
    if (_pcspeaker.mode != PCSPEAKER_DATA) {
        return;
    }
    _pcspeaker.data_ticks += pt_sys.timer->get_hires() ? 1 : TIMER_HIRES_DIV;
    if (_pcspeaker.data_ticks >= _pcspeaker.data_rate) {
        _pcspeaker.offset += 1;
        _pcspeaker.data_ticks %= _pcspeaker.data_rate;
    }

    if (_pcspeaker.offset >= _pcspeaker.data_count) {
        pcspeaker_stop();
    } else {
        uint16_t value = _pcspeaker.data[_pcspeaker.offset];
        if (value == 0) {
            pt_sys.beep->set_gate(false);
        } else if (value == 0xffff) {
            pcspeaker_stop();
        } else {
            pcspeaker_tone_raw(value);
        }
    }
}

void pcspeaker_stop()
{
    pt_sys.timer->set_hires(false);
    _pcspeaker.mode = PCSPEAKER_OFF;
    if (_pcspeaker.sample_data) {
        free(_pcspeaker.sample_data);
        _pcspeaker.sample_data = NULL;
        _pcspeaker.sample_len = 0;
    }
    if (_pcspeaker.data) {
        free(_pcspeaker.data);
        _pcspeaker.data = NULL;
        _pcspeaker.data_count = 0;
    }
    _pcspeaker.data_rate = 0;
    _pcspeaker.data_ticks = 0;
    _pcspeaker.offset = 0;
    pt_sys.beep->set_gate(false);
}

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
        // log_print("load_pc_speaker_ifs: %d - name: %s, len: %d\n", i, name, data_len);
    }
    result->sounds_len = sfx_count;

    fclose(fp);
    return result;
}
