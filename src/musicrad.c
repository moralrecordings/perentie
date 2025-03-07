#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef SYSTEM_DOS
#include "dos.h"
#include <dpmi.h>
#endif

#include "musicrad.h"
#include "system.h"
#include "utils.h"

// Rough C port of the Reality Adlib Tracker 2.0a player example code.

#ifndef RAD_DETECT_REPEATS
#define RAD_DETECT_REPEATS 0
#endif

// Various constants
enum {
    kTracks = 100,
    kChannels = 9,
    kTrackLines = 64,
    kRiffTracks = 10,
    kInstruments = 127,

    cmPortamentoUp = 0x1,
    cmPortamentoDwn = 0x2,
    cmToneSlide = 0x3,
    cmToneVolSlide = 0x5,
    cmVolSlide = 0xA,
    cmSetVol = 0xC,
    cmJumpToLine = 0xD,
    cmSetSpeed = 0xF,
    cmIgnore = ('I' - 55),
    cmMultiplier = ('M' - 55),
    cmRiff = ('R' - 55),
    cmTranspose = ('T' - 55),
    cmFeedback = ('U' - 55),
    cmVolume = ('V' - 55),
};

enum e_Source {
    SNone,
    SRiff,
    SIRiff,
};

enum {
    fKeyOn = 1 << 0,
    fKeyOff = 1 << 1,
    fKeyedOn = 1 << 2,
};

typedef struct {
    uint8_t Feedback[2];
    uint8_t Panning[2];
    uint8_t Algorithm;
    uint8_t Detune;
    uint8_t Volume;
    uint8_t RiffSpeed;
    uint8_t* Riff;
    uint8_t Operators[4][5];
} CInstrument;

typedef struct {
    int8_t PortSlide;
    int8_t VolSlide;
    uint16_t ToneSlideFreq;
    uint8_t ToneSlideOct;
    uint8_t ToneSlideSpeed;
    int8_t ToneSlideDir;
} CEffects;

typedef struct {
    CEffects FX;
    uint8_t* Track;
    uint8_t* TrackStart;
    uint8_t Line;
    uint8_t Speed;
    uint8_t SpeedCnt;
    int8_t TransposeOctave;
    int8_t TransposeNote;
    uint8_t LastInstrument;
} CRiff;

typedef struct {
    uint8_t LastInstrument;
    CInstrument* Instrument;
    uint8_t Volume;
    uint8_t DetuneA;
    uint8_t DetuneB;
    uint8_t KeyFlags;
    uint16_t CurrFreq;
    int8_t CurrOctave;
    CEffects FX;
    CRiff Riff, IRiff;
} CChannel;

struct RADPlayer {
    byte Data[65536];
    CInstrument Instruments[kInstruments];
    CChannel Channels[kChannels];
    uint32_t PlayTime;
#if RAD_DETECT_REPEATS
    uint32_t OrderMap[4];
    bool Repeating;
#endif
    int16_t Hertz;
    uint8_t* OrderList;
    uint8_t* Tracks[kTracks];
    uint8_t* Riffs[kRiffTracks][kChannels];
    uint8_t* Track;
    bool Initialised;
    bool Playing;
    uint8_t Speed;
    uint8_t OrderListSize;
    uint8_t SpeedCnt;
    uint8_t Order;
    uint8_t Line;
    int8_t Entrances;
    uint8_t MasterVol;
    int8_t LineJump;
    uint8_t OPL3Regs[512];

    // Values exported by UnpackNote()
    int8_t NoteNum;
    int8_t OctaveNum;
    uint8_t InstNum;
    uint8_t EffectNum;
    uint8_t Param;
    bool LastNote;
};
typedef struct RADPlayer RADPlayer;

static RADPlayer rad_player = { 0 };
static uint32_t rad_timer = 0;

//--------------------------------------------------------------------------------------------------
// clang-format off
const int8_t NoteSize[] = { 0, 2, 1, 3, 1, 3, 2, 4 };
const uint16_t ChanOffsets3[9] = { 0, 1, 2, 0x100, 0x101, 0x102, 6, 7, 8 };              // OPL3 first channel
const uint16_t Chn2Offsets3[9] = { 3, 4, 5, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108 };  // OPL3 second channel
const uint16_t NoteFreq[] = { 0x16b,0x181,0x198,0x1b0,0x1ca,0x1e5,0x202,0x220,0x241,0x263,0x287,0x2ae };
const uint16_t OpOffsets3[9][4] = {
    {  0x00B, 0x008, 0x003, 0x000  },
    {  0x00C, 0x009, 0x004, 0x001  },
    {  0x00D, 0x00A, 0x005, 0x002  },
    {  0x10B, 0x108, 0x103, 0x100  },
    {  0x10C, 0x109, 0x104, 0x101  },
    {  0x10D, 0x10A, 0x105, 0x102  },
    {  0x113, 0x110, 0x013, 0x010  },
    {  0x114, 0x111, 0x014, 0x011  },
    {  0x115, 0x112, 0x015, 0x012  }
};
const bool AlgCarriers[7][4] = {
    {  true, false, false, false  },  // 0 - 2op - op < op
    {  true, true,  false, false  },  // 1 - 2op - op + op
    {  true, false, false, false  },  // 2 - 4op - op < op < op < op
    {  true, false, false, true   },  // 3 - 4op - op < op < op + op
    {  true, false, true,  false  },  // 4 - 4op - op < op + op < op
    {  true, false, true,  true   },  // 5 - 4op - op < op + op + op
    {  true, true,  true,  true   },  // 6 - 4op - op + op + op + op
};
// clang-format on

uint8_t* rad_get_track(RADPlayer* rad);
void rad_play_line(RADPlayer* rad);
void rad_play_note(RADPlayer* rad, int channum, int8_t notenum, int8_t octave, uint16_t instnum, uint8_t cmd,
    uint8_t param, enum e_Source src, int op);
void rad_play_note_opl3(RADPlayer* rad, int channum, int8_t octave, int8_t note);
void rad_reset_fx(RADPlayer* rad, CEffects* fx);
void rad_tick_riff(RADPlayer* rad, int channum, CRiff* riff, bool chan_riff);
void rad_continue_fx(RADPlayer* rad, int channum, CEffects* fx);
void rad_set_volume(RADPlayer* rad, int channum, uint8_t vol);
void rad_get_slide_dir(RADPlayer* rad, int channum, CEffects* fx);
void rad_load_inst_multiplier_opl3(RADPlayer* rad, int channum, int op, uint8_t mult);
void rad_load_inst_volume_opl3(RADPlayer* rad, int channum, int op, uint8_t vol);
void rad_load_inst_feedback_opl3(RADPlayer* rad, int channum, int which, uint8_t fb);
void rad_load_instrument_opl3(RADPlayer* rad, int channum);
void rad_set_opl3(RADPlayer* rad, uint16_t reg, uint8_t val);
uint8_t rad_get_opl3(RADPlayer* rad, uint16_t reg);
void rad_portamento(RADPlayer* rad, uint16_t channum, CEffects* fx, int8_t amount, bool toneslide);
void rad_transpose(RADPlayer* rad, int8_t note, int8_t octave);

void radplayer_shutdown()
{
    rad_stop(&rad_player);
    pt_sys.timer->remove_callback(rad_timer);
    rad_timer = 0;
}

uint32_t radplayer_callback(uint32_t interval, void* data)
{
    rad_update(&rad_player);
    return interval;
}

bool radplayer_load_file(const char* path)
{
    rad_stop(&rad_player);

    FILE* fp = fopen(path, "rb");
    if (!fp)
        return false;
    // get the file size
    fseek(fp, 0, SEEK_END);
    size_t rad_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (rad_size > 65536) {
        // too big for the music buffer!
        fclose(fp);
        return false;
    }
    fread(rad_player.Data, rad_size, 1, fp);
    fclose(fp);
    rad_load(&rad_player);

    if (rad_timer)
        pt_sys.timer->remove_callback(rad_timer);

    rad_timer = pt_sys.timer->add_callback(1000 / rad_get_hertz(&rad_player), radplayer_callback, NULL);

    return true;
}

void radplayer_play()
{
    rad_play(&rad_player);
}

void radplayer_stop()
{
    rad_stop(&rad_player);
}

void radplayer_set_master_volume(int vol)
{
    rad_set_master_volume(&rad_player, vol);
}

int radplayer_get_master_volume()
{
    return rad_get_master_volume(&rad_player);
}

void radplayer_set_position(int order, int line)
{
    rad_set_position(&rad_player, order, line);
}

int radplayer_get_order()
{
    return rad_get_order(&rad_player);
}

int radplayer_get_line()
{
    return rad_get_line(&rad_player);
}

//==================================================================================================
// Initialise a RAD tune for playback.  This assumes the tune data is valid and does minimal data
// checking.
//==================================================================================================
void rad_load(RADPlayer* rad)
{
    if (!rad)
        return;

    rad->Initialised = false;
    rad->Playing = false;

    // Version check; we only support version 2.1 tune files
    if (*((uint8_t*)rad->Data + 0x10) != 0x21) {
        rad->Hertz = -1;
        return;
    }

    for (int i = 0; i < kTracks; i++)
        rad->Tracks[i] = 0;

    for (int i = 0; i < kRiffTracks; i++)
        for (int j = 0; j < kChannels; j++)
            rad->Riffs[i][j] = 0;

    uint8_t* s = (uint8_t*)rad->Data + 0x11;

    uint8_t flags = *s++;
    rad->Speed = flags & 0x1F;

    // Is BPM value present?
    rad->Hertz = 50;
    if (flags & 0x20) {
        rad->Hertz = (s[0] | ((int)(s[1]) << 8)) * 2 / 5;
        s += 2;
    }

    // Slow timer tune?  Return an approximate hz
    if (flags & 0x40)
        rad->Hertz = 18;

    // Skip any description
    while (*s)
        s++;
    s++;

    // Unpack the instruments
    while (1) {

        // Instrument number, 0 indicates end of list
        uint8_t inst_num = *s++;
        if (inst_num == 0)
            break;

        // Skip instrument name
        s += *s++;

        CInstrument* inst = &rad->Instruments[inst_num - 1];

        uint8_t alg = *s++;
        inst->Algorithm = alg & 7;
        inst->Panning[0] = (alg >> 3) & 3;
        inst->Panning[1] = (alg >> 5) & 3;

        if (inst->Algorithm < 7) {

            uint8_t b = *s++;
            inst->Feedback[0] = b & 15;
            inst->Feedback[1] = b >> 4;

            b = *s++;
            inst->Detune = b >> 4;
            inst->RiffSpeed = b & 15;

            inst->Volume = *s++;

            for (int i = 0; i < 4; i++) {
                uint8_t* op = inst->Operators[i];
                for (int j = 0; j < 5; j++)
                    op[j] = *s++;
            }

        } else {

            // Ignore MIDI instrument data
            s += 6;
        }

        // Instrument riff?
        if (alg & 0x80) {
            int size = s[0] | ((int)(s[1]) << 8);
            s += 2;
            inst->Riff = s;
            s += size;
        } else
            inst->Riff = 0;
    }

    // Get order list
    rad->OrderListSize = *s++;
    rad->OrderList = s;
    s += rad->OrderListSize;

    // Locate the tracks
    while (1) {

        // Track number
        uint8_t track_num = *s++;
        if (track_num >= kTracks)
            break;

        // Track size in bytes
        int size = s[0] | ((int)(s[1]) << 8);
        s += 2;

        rad->Tracks[track_num] = s;
        s += size;
    }

    // Locate the riffs
    while (1) {

        // Riff id
        uint8_t riffid = *s++;
        uint8_t riffnum = riffid >> 4;
        uint8_t channum = riffid & 15;
        if (riffnum >= kRiffTracks || channum > kChannels)
            break;

        // Track size in bytes
        int size = s[0] | ((int)(s[1]) << 8);
        s += 2;

        rad->Riffs[riffnum][channum - 1] = s;
        s += size;
    }

    // Done parsing tune, now set up for play
    for (int i = 0; i < 512; i++)
        rad->OPL3Regs[i] = 255;

    rad->Initialised = true;
    rad_stop(rad); // zero registers, set OPL3 flags
}

void rad_play(RADPlayer* rad)
{
    if (!rad->Initialised)
        return;

    rad->Playing = true;
}

//==================================================================================================
// Stop all sounds and reset the tune.  Tune will play from the beginning again if you continue to
// Update().
//==================================================================================================
void rad_stop(RADPlayer* rad)
{
    if (!rad->Initialised)
        return;

    // Clear all registers
    for (uint16_t reg = 0x20; reg < 0xF6; reg++) {

        // Ensure envelopes decay all the way
        uint8_t val = (reg >= 0x60 && reg < 0xA0) ? 0xFF : 0;

        rad_set_opl3(rad, reg, val);
        rad_set_opl3(rad, reg + 0x100, val);
    }

    // Configure OPL3
    rad_set_opl3(rad, 1, 0x20); // Allow waveforms
    rad_set_opl3(rad, 8, 0); // No split point
    rad_set_opl3(rad, 0xbd, 0); // No drums, etc.
    rad_set_opl3(rad, 0x104, 0); // Everything 2-op by default
    rad_set_opl3(rad, 0x105, 1); // OPL3 mode on

#if RAD_DETECT_REPEATS
    // The order map keeps track of which patterns we've played so we can detect when the tune
    // starts to repeat.  Jump markers can't be reliably used for this
    PlayTime = 0;
    Repeating = false;
    for (int i = 0; i < 4; i++)
        OrderMap[i] = 0;
#endif

    // Initialise play values
    rad->SpeedCnt = 1;
    rad->Order = 0;
    rad->Track = rad_get_track(rad);
    rad->Line = 0;
    rad->Entrances = 0;
    rad->MasterVol = 64;

    // Initialise channels
    for (int i = 0; i < kChannels; i++) {
        CChannel* chan = &rad->Channels[i];
        chan->LastInstrument = 0;
        chan->Instrument = 0;
        chan->Volume = 0;
        chan->DetuneA = 0;
        chan->DetuneB = 0;
        chan->KeyFlags = 0;
        chan->Riff.SpeedCnt = 0;
        chan->IRiff.SpeedCnt = 0;
    }

    rad->Playing = false;
}

//==================================================================================================
// Playback update.  Call BPM * 2 / 5 times a second.  Use GetHertz() for this number after the
// tune has been initialised.  Returns true if tune is starting to repeat.
//==================================================================================================
bool rad_update(RADPlayer* rad)
{
    if (!rad)
        return false;

    if (!rad->Initialised)
        return false;

    if (!rad->Playing)
        return false;

    // Run riffs
    for (int i = 0; i < kChannels; i++) {
        CChannel* chan = &rad->Channels[i];
        rad_tick_riff(rad, i, &chan->IRiff, false);
        rad_tick_riff(rad, i, &chan->Riff, true);
    }

    // Run main track
    rad_play_line(rad);

    // Run effects
    for (int i = 0; i < kChannels; i++) {
        CChannel* chan = &rad->Channels[i];
        rad_continue_fx(rad, i, &chan->IRiff.FX);
        rad_continue_fx(rad, i, &chan->Riff.FX);
        rad_continue_fx(rad, i, &chan->FX);
    }

    // Update play time.  We convert to seconds when queried
    rad->PlayTime++;

#if RAD_DETECT_REPEATS
    return rad->Repeating;
#else
    return false;
#endif
}

int rad_get_hertz(RADPlayer* rad)
{
    return rad->Hertz;
}

int rad_get_play_time_in_seconds(RADPlayer* rad)
{
    return rad->PlayTime / rad->Hertz;
}

int rad_get_tune_pos(RADPlayer* rad)
{
    return rad->Order;
}

int rad_get_tune_length(RADPlayer* rad)
{
    return rad->OrderListSize;
}

int rad_get_tune_line(RADPlayer* rad)
{
    return rad->Line;
}

void rad_set_master_volume(RADPlayer* rad, int vol)
{
    vol = MAX(MIN(vol, 64), 0);
    rad->MasterVol = vol;
    if (rad->Playing) {
        for (int i = 0; i < kChannels; i++) {
            rad_set_volume(rad, i, rad->Channels[i].Volume);
        }
    }
}

int rad_get_master_volume(RADPlayer* rad)
{
    return rad->MasterVol;
}

int rad_get_speed(RADPlayer* rad)
{
    return rad->Speed;
}

void rad_set_position(RADPlayer* rad, int order, int line)
{
    rad_stop(rad);
    rad->Order = order;
    rad->Track = rad_get_track(rad);
    rad->Line = line;
    rad_play(rad);
}

int rad_get_order(RADPlayer* rad)
{
    return rad->Order;
}

int rad_get_line(RADPlayer* rad)
{
    return rad->Line;
}

//==================================================================================================
// Unpacks a single RAD note.
//==================================================================================================
bool rad_unpack_note(RADPlayer* rad, uint8_t** s, uint8_t* last_instrument)
{

    uint8_t chanid = *(*s)++;

    rad->InstNum = 0;
    rad->EffectNum = 0;
    rad->Param = 0;

    // Unpack note data
    uint8_t note = 0;
    if (chanid & 0x40) {
        uint8_t n = *(*s)++;
        note = n & 0x7F;

        // Retrigger last instrument?
        if (n & 0x80)
            rad->InstNum = *last_instrument;
    }

    // Do we have an instrument?
    if (chanid & 0x20) {
        rad->InstNum = *(*s)++;
        *last_instrument = rad->InstNum;
    }

    // Do we have an effect?
    if (chanid & 0x10) {
        rad->EffectNum = *(*s)++;
        rad->Param = *(*s)++;
    }

    rad->NoteNum = note & 15;
    rad->OctaveNum = note >> 4;

    return ((chanid & 0x80) != 0);
}

//==================================================================================================
// Get current track as indicated by order list.
//==================================================================================================
uint8_t* rad_get_track(RADPlayer* rad)
{

    // If at end of tune start again from beginning
    if (rad->Order >= rad->OrderListSize)
        rad->Order = 0;

    uint8_t track_num = rad->OrderList[rad->Order];

    // Jump marker?  Note, we don't recognise multiple jump markers as that could put us into an
    // infinite loop
    if (track_num & 0x80) {
        rad->Order = track_num & 0x7F;
        track_num = rad->OrderList[rad->Order] & 0x7F;
    }

#if RAD_DETECT_REPEATS
    // Check for tune repeat, and mark order in order map
    if (Order < 128) {
        int byte = rad->Order >> 5;
        uint32_t bit = uint32_t(1) << (rad->Order & 31);
        if (rad->OrderMap[byte] & bit)
            rad->Repeating = true;
        else
            rad->OrderMap[byte] |= bit;
    }
#endif

    return rad->Tracks[track_num];
}

//==================================================================================================
// Skip through track till we reach the given line or the next higher one.  Returns null if none.
//==================================================================================================
uint8_t* rad_skip_to_line(RADPlayer* rad, uint8_t* trk, uint8_t linenum, bool chan_riff)
{

    while (1) {

        uint8_t lineid = *trk;
        if ((lineid & 0x7F) >= linenum)
            return trk;
        if (lineid & 0x80)
            break;
        trk++;

        // Skip channel notes
        uint8_t chanid;
        do {
            chanid = *trk++;
            trk += NoteSize[(chanid >> 4) & 7];
        } while (!(chanid & 0x80) && !chan_riff);
    }

    return 0;
}

//==================================================================================================
// Plays one line of current track and advances pointers.
//==================================================================================================
void rad_play_line(RADPlayer* rad)
{

    rad->SpeedCnt--;
    if (rad->SpeedCnt > 0)
        return;
    rad->SpeedCnt = rad->Speed;

    // Reset channel effects
    for (int i = 0; i < kChannels; i++)
        rad_reset_fx(rad, &rad->Channels[i].FX);

    rad->LineJump = -1;

    // At the right line?
    uint8_t* trk = rad->Track;
    if (trk && (*trk & 0x7F) <= rad->Line) {
        uint8_t lineid = *trk++;

        // Run through channels
        bool last;
        do {
            int channum = *trk & 15;
            CChannel* chan = &rad->Channels[channum];
            last = rad_unpack_note(rad, &trk, &chan->LastInstrument);
            rad_play_note(rad, channum, rad->NoteNum, rad->OctaveNum, rad->InstNum, rad->EffectNum, rad->Param, 0, 0);
        } while (!last);

        // Was this the last line?
        if (lineid & 0x80)
            trk = 0;

        rad->Track = trk;
    }

    // Move to next line
    rad->Line++;
    if (rad->Line >= kTrackLines || rad->LineJump >= 0) {

        if (rad->LineJump >= 0)
            rad->Line = rad->LineJump;
        else
            rad->Line = 0;

        // Move to next track in order list
        rad->Order++;
        rad->Track = rad_get_track(rad);
    }
}

//==================================================================================================
// Play a single note.  Returns the line number in the next pattern to jump to if a jump command was
// found, or -1 if none.
//==================================================================================================
void rad_play_note(RADPlayer* rad, int channum, int8_t notenum, int8_t octave, uint16_t instnum, uint8_t cmd,
    uint8_t param, enum e_Source src, int op)
{
    CChannel* chan = &rad->Channels[channum];

    // Recursion detector.  This is needed as riffs can trigger other riffs, and they could end up
    // in a loop
    if (rad->Entrances >= 8)
        return;
    rad->Entrances++;

    // Select which effects source we're using
    CEffects* fx = &chan->FX;
    if (src == SRiff)
        fx = &chan->Riff.FX;
    else if (src == SIRiff)
        fx = &chan->IRiff.FX;

    bool transposing = false;

    // For tone-slides the note is the target
    if (cmd == cmToneSlide) {
        if (notenum > 0 && notenum <= 12) {
            fx->ToneSlideOct = octave;
            fx->ToneSlideFreq = NoteFreq[notenum - 1];
        }
        goto toneslide;
    }

    // Playing a new instrument?
    if (instnum > 0) {
        CInstrument* oldinst = chan->Instrument;
        CInstrument* inst = &rad->Instruments[instnum - 1];
        chan->Instrument = inst;

        // Ignore MIDI instruments
        if (inst->Algorithm == 7) {
            rad->Entrances--;
            return;
        }

        rad_load_instrument_opl3(rad, channum);

        // Bounce the channel
        chan->KeyFlags |= fKeyOff | fKeyOn;

        rad_reset_fx(rad, &chan->IRiff.FX);

        if (src != SIRiff || inst != oldinst) {

            // Instrument riff?
            if (inst->Riff && inst->RiffSpeed > 0) {

                chan->IRiff.Track = chan->IRiff.TrackStart = inst->Riff;
                chan->IRiff.Line = 0;
                chan->IRiff.Speed = inst->RiffSpeed;
                chan->IRiff.LastInstrument = 0;

                // Note given with riff command is used to transpose the riff
                if (notenum >= 1 && notenum <= 12) {
                    chan->IRiff.TransposeOctave = octave;
                    chan->IRiff.TransposeNote = notenum;
                    transposing = true;
                } else {
                    chan->IRiff.TransposeOctave = 3;
                    chan->IRiff.TransposeNote = 12;
                }

                // Do first tick of riff
                chan->IRiff.SpeedCnt = 1;
                rad_tick_riff(rad, channum, &chan->IRiff, false);

            } else
                chan->IRiff.SpeedCnt = 0;
        }
    }

    // Starting a channel riff?
    if (cmd == cmRiff || cmd == cmTranspose) {

        rad_reset_fx(rad, &chan->Riff.FX);

        uint8_t p0 = param / 10;
        uint8_t p1 = param % 10;
        chan->Riff.Track = p1 > 0 ? rad->Riffs[p0][p1 - 1] : 0;
        if (chan->Riff.Track) {

            chan->Riff.TrackStart = chan->Riff.Track;
            chan->Riff.Line = 0;
            chan->Riff.Speed = rad->Speed;
            chan->Riff.LastInstrument = 0;

            // Note given with riff command is used to transpose the riff
            if (cmd == cmTranspose && notenum >= 1 && notenum <= 12) {
                chan->Riff.TransposeOctave = octave;
                chan->Riff.TransposeNote = notenum;
                transposing = true;
            } else {
                chan->Riff.TransposeOctave = 3;
                chan->Riff.TransposeNote = 12;
            }

            // Do first tick of riff
            chan->Riff.SpeedCnt = 1;
            rad_tick_riff(rad, channum, &chan->Riff, true);

        } else
            chan->Riff.SpeedCnt = 0;
    }

    // Play the note
    if (!transposing && notenum > 0) {

        // Key-off?
        if (notenum == 15)
            chan->KeyFlags |= fKeyOff;

        if (!chan->Instrument || chan->Instrument->Algorithm < 7)
            rad_play_note_opl3(rad, channum, octave, notenum);
    }

    // Process effect
    switch (cmd) {

    case cmSetVol:
        rad_set_volume(rad, channum, param);
        break;

    case cmSetSpeed:
        if (src == SNone) {
            rad->Speed = param;
            rad->SpeedCnt = param;
        } else if (src == SRiff) {
            chan->Riff.Speed = param;
            chan->Riff.SpeedCnt = param;
        } else if (src == SIRiff) {
            chan->IRiff.Speed = param;
            chan->IRiff.SpeedCnt = param;
        }
        break;

    case cmPortamentoUp:
        fx->PortSlide = param;
        break;

    case cmPortamentoDwn:
        fx->PortSlide = -(int8_t)(param);
        break;

    case cmToneVolSlide:
    case cmVolSlide: {
        int8_t val = param;
        if (val >= 50)
            val = -(val - 50);
        fx->VolSlide = val;
        if (cmd != cmToneVolSlide)
            break;
    }
        // Fall through!

    case cmToneSlide: {
    toneslide:
        uint8_t speed = param;
        if (speed)
            fx->ToneSlideSpeed = speed;
        rad_get_slide_dir(rad, channum, fx);
        break;
    }

    case cmJumpToLine: {
        if (param >= kTrackLines)
            break;

        // Note: jump commands in riffs are checked for within TickRiff()
        if (src == SNone)
            rad->LineJump = param;

        break;
    }

    case cmMultiplier: {
        if (src == SIRiff)
            rad_load_inst_multiplier_opl3(rad, channum, op, param);
        break;
    }

    case cmVolume: {
        if (src == SIRiff)
            rad_load_inst_volume_opl3(rad, channum, op, param);
        break;
    }

    case cmFeedback: {
        if (src == SIRiff) {
            uint8_t which = param / 10;
            uint8_t fb = param % 10;
            rad_load_inst_feedback_opl3(rad, channum, which, fb);
        }
        break;
    }
    }

    rad->Entrances--;
}

//==================================================================================================
// Sets the OPL3 registers for a given instrument.
//==================================================================================================
void rad_load_instrument_opl3(RADPlayer* rad, int channum)
{
    CChannel* chan = &rad->Channels[channum];

    const CInstrument* inst = chan->Instrument;
    if (!inst)
        return;

    uint8_t alg = inst->Algorithm;
    chan->Volume = inst->Volume;
    chan->DetuneA = (inst->Detune + 1) >> 1;
    chan->DetuneB = inst->Detune >> 1;

    // Turn on 4-op mode for algorithms 2 and 3 (algorithms 4 to 6 are simulated with 2-op mode)
    if (channum < 6) {
        uint8_t mask = 1 << channum;
        rad_set_opl3(rad, 0x104, (rad_get_opl3(rad, 0x104) & ~mask) | (alg == 2 || alg == 3 ? mask : 0));
    }

    // Left/right/feedback/algorithm
    rad_set_opl3(rad, 0xC0 + ChanOffsets3[channum],
        ((inst->Panning[1] ^ 3) << 4) | inst->Feedback[1] << 1 | (alg == 3 || alg == 5 || alg == 6 ? 1 : 0));
    rad_set_opl3(rad, 0xC0 + Chn2Offsets3[channum],
        ((inst->Panning[0] ^ 3) << 4) | inst->Feedback[0] << 1 | (alg == 1 || alg == 6 ? 1 : 0));

    // Load the operators
    for (int i = 0; i < 4; i++) {

        static const uint8_t blank[] = { 0, 0x3F, 0, 0xF0, 0 };
        const uint8_t* op = (alg < 2 && i >= 2) ? blank : inst->Operators[i];
        uint16_t reg = OpOffsets3[channum][i];

        uint16_t vol = ~op[1] & 0x3F;

        // Do volume scaling for carriers
        if (AlgCarriers[alg][i]) {
            vol = vol * inst->Volume / 64;
            vol = vol * rad->MasterVol / 64;
        }

        rad_set_opl3(rad, reg + 0x20, op[0]);
        rad_set_opl3(rad, reg + 0x40, (op[1] & 0xC0) | ((vol ^ 0x3F) & 0x3F));
        rad_set_opl3(rad, reg + 0x60, op[2]);
        rad_set_opl3(rad, reg + 0x80, op[3]);
        rad_set_opl3(rad, reg + 0xE0, op[4]);
    }
}

//==================================================================================================
// Play note on OPL3 hardware.
//==================================================================================================
void rad_play_note_opl3(RADPlayer* rad, int channum, int8_t octave, int8_t note)
{
    CChannel* chan = &rad->Channels[channum];

    uint16_t o1 = ChanOffsets3[channum];
    uint16_t o2 = Chn2Offsets3[channum];

    // Key off the channel
    if (chan->KeyFlags & fKeyOff) {
        chan->KeyFlags &= ~(fKeyOff | fKeyedOn);
        rad_set_opl3(rad, 0xB0 + o1, rad_get_opl3(rad, 0xB0 + o1) & ~0x20);
        rad_set_opl3(rad, 0xB0 + o2, rad_get_opl3(rad, 0xB0 + o2) & ~0x20);
    }

    if (note == 15)
        return;

    bool op4 = (chan->Instrument && chan->Instrument->Algorithm >= 2);

    uint16_t freq = NoteFreq[note - 1];
    uint16_t frq2 = freq;

    chan->CurrFreq = freq;
    chan->CurrOctave = octave;

    // Detune.  We detune both channels in the opposite direction so the note retains its tuning
    freq += chan->DetuneA;
    frq2 -= chan->DetuneB;

    // Frequency low byte
    if (op4)
        rad_set_opl3(rad, 0xA0 + o1, frq2 & 0xFF);
    rad_set_opl3(rad, 0xA0 + o2, freq & 0xFF);

    // Frequency high bits + octave + key on
    if (chan->KeyFlags & fKeyOn)
        chan->KeyFlags = (chan->KeyFlags & ~fKeyOn) | fKeyedOn;
    if (op4)
        rad_set_opl3(rad, 0xB0 + o1, (frq2 >> 8) | (octave << 2) | ((chan->KeyFlags & fKeyedOn) ? 0x20 : 0));
    else
        rad_set_opl3(rad, 0xB0 + o1, 0);
    rad_set_opl3(rad, 0xB0 + o2, (freq >> 8) | (octave << 2) | ((chan->KeyFlags & fKeyedOn) ? 0x20 : 0));
}

//==================================================================================================
// Prepare FX for new line.
//==================================================================================================
void rad_reset_fx(RADPlayer* rad, CEffects* fx)
{
    fx->PortSlide = 0;
    fx->VolSlide = 0;
    fx->ToneSlideDir = 0;
}

//==================================================================================================
// Tick the channel riff.
//==================================================================================================
void rad_tick_riff(RADPlayer* rad, int channum, CRiff* riff, bool chan_riff)
{
    uint8_t lineid;

    if (riff->SpeedCnt == 0) {
        rad_reset_fx(rad, &riff->FX);
        return;
    }

    riff->SpeedCnt--;
    if (riff->SpeedCnt > 0)
        return;
    riff->SpeedCnt = riff->Speed;

    uint8_t line = riff->Line++;
    if (riff->Line >= kTrackLines)
        riff->SpeedCnt = 0;

    rad_reset_fx(rad, &riff->FX);

    // Is this the current line in track?
    uint8_t* trk = riff->Track;
    if (trk && (*trk & 0x7F) == line) {
        lineid = *trk++;

        if (chan_riff) {

            // Channel riff: play current note
            rad_unpack_note(rad, &trk, &riff->LastInstrument);
            rad_transpose(rad, riff->TransposeNote, riff->TransposeOctave);
            rad_play_note(
                rad, channum, rad->NoteNum, rad->OctaveNum, rad->InstNum, rad->EffectNum, rad->Param, SRiff, 0);

        } else {

            // Instrument riff: here each track channel is an extra effect that can run, but is not
            // actually a different physical channel
            bool last;
            do {
                int col = *trk & 15;
                last = rad_unpack_note(rad, &trk, &riff->LastInstrument);
                if (rad->EffectNum != cmIgnore)
                    rad_transpose(rad, riff->TransposeNote, riff->TransposeOctave);
                rad_play_note(rad, channum, rad->NoteNum, rad->OctaveNum, rad->InstNum, rad->EffectNum, rad->Param,
                    SIRiff, col > 0 ? (col - 1) & 3 : 0);
            } while (!last);
        }

        // Last line?
        if (lineid & 0x80)
            trk = 0;

        riff->Track = trk;
    }

    // Special case; if next line has a jump command, run it now
    if (!trk || (*trk++ & 0x7F) != riff->Line)
        return;

    rad_unpack_note(rad, &trk, &lineid); // lineid is just a dummy here
    if (rad->EffectNum == cmJumpToLine && rad->Param < kTrackLines) {
        riff->Line = rad->Param;
        riff->Track = rad_skip_to_line(rad, riff->TrackStart, rad->Param, chan_riff);
    }
}

//==================================================================================================
// This continues any effects that operate continuously (eg. slides).
//==================================================================================================
void rad_continue_fx(RADPlayer* rad, int channum, CEffects* fx)
{
    CChannel* chan = &rad->Channels[channum];

    if (fx->PortSlide)
        rad_portamento(rad, channum, fx, fx->PortSlide, false);

    if (fx->VolSlide) {
        int8_t vol = chan->Volume;
        vol -= fx->VolSlide;
        if (vol < 0)
            vol = 0;
        rad_set_volume(rad, channum, vol);
    }

    if (fx->ToneSlideDir)
        rad_portamento(rad, channum, fx, fx->ToneSlideDir, true);
}

//==================================================================================================
// Sets the volume of given channel.
//==================================================================================================
void rad_set_volume(RADPlayer* rad, int channum, uint8_t vol)
{
    CChannel* chan = &rad->Channels[channum];

    // Ensure volume is within range
    if (vol > 64)
        vol = 64;

    chan->Volume = vol;

    // Scale volume to master volume
    vol = vol * rad->MasterVol / 64;

    CInstrument* inst = chan->Instrument;
    if (!inst)
        return;
    uint8_t alg = inst->Algorithm;

    // Set volume of all carriers
    for (int i = 0; i < 4; i++) {
        uint8_t* op = inst->Operators[i];

        // Is this operator a carrier?
        if (!AlgCarriers[alg][i])
            continue;

        uint8_t opvol = (uint16_t)((op[1] & 63) ^ 63) * vol / 64;
        uint16_t reg = 0x40 + OpOffsets3[channum][i];
        rad_set_opl3(rad, reg, (rad_get_opl3(rad, reg) & 0xC0) | (opvol ^ 0x3F));
    }
}

//==================================================================================================
// Starts a tone-slide.
//==================================================================================================
void rad_get_slide_dir(RADPlayer* rad, int channum, CEffects* fx)
{
    CChannel* chan = &rad->Channels[channum];

    int8_t speed = fx->ToneSlideSpeed;
    if (speed > 0) {
        uint8_t oct = fx->ToneSlideOct;
        uint16_t freq = fx->ToneSlideFreq;

        uint16_t oldfreq = chan->CurrFreq;
        uint8_t oldoct = chan->CurrOctave;

        if (oldoct > oct)
            speed = -speed;
        else if (oldoct == oct) {
            if (oldfreq > freq)
                speed = -speed;
            else if (oldfreq == freq)
                speed = 0;
        }
    }

    fx->ToneSlideDir = speed;
}

//==================================================================================================
// Load multiplier value into operator.
//==================================================================================================
void rad_load_inst_multiplier_opl3(RADPlayer* rad, int channum, int op, uint8_t mult)
{
    uint16_t reg = 0x20 + OpOffsets3[channum][op];
    rad_set_opl3(rad, reg, (rad_get_opl3(rad, reg) & 0xF0) | (mult & 15));
}

//==================================================================================================
// Load volume value into operator.
//==================================================================================================
void rad_load_inst_volume_opl3(RADPlayer* rad, int channum, int op, uint8_t vol)
{
    uint16_t reg = 0x40 + OpOffsets3[channum][op];
    rad_set_opl3(rad, reg, (rad_get_opl3(rad, reg) & 0xC0) | ((vol & 0x3F) ^ 0x3F));
}

//==================================================================================================
// Load feedback value into instrument.
//==================================================================================================
void rad_load_inst_feedback_opl3(RADPlayer* rad, int channum, int which, uint8_t fb)
{

    if (which == 0) {

        uint16_t reg = 0xC0 + Chn2Offsets3[channum];
        rad_set_opl3(rad, reg, (rad_get_opl3(rad, reg) & 0x31) | ((fb & 7) << 1));

    } else if (which == 1) {

        uint16_t reg = 0xC0 + ChanOffsets3[channum];
        rad_set_opl3(rad, reg, (rad_get_opl3(rad, reg) & 0x31) | ((fb & 7) << 1));
    }
}

//==================================================================================================
// This adjusts the pitch of the given channel's note.  There may also be a limiting value on the
// portamento (for tone slides).
//==================================================================================================
void rad_portamento(RADPlayer* rad, uint16_t channum, CEffects* fx, int8_t amount, bool toneslide)
{
    CChannel* chan = &rad->Channels[channum];

    uint16_t freq = chan->CurrFreq;
    uint8_t oct = chan->CurrOctave;

    freq += amount;

    if (freq < 0x156) {

        if (oct > 0) {
            oct--;
            freq += 0x2AE - 0x156;
        } else
            freq = 0x156;

    } else if (freq > 0x2AE) {

        if (oct < 7) {
            oct++;
            freq -= 0x2AE - 0x156;
        } else
            freq = 0x2AE;
    }

    if (toneslide) {

        if (amount >= 0) {

            if (oct > fx->ToneSlideOct || (oct == fx->ToneSlideOct && freq >= fx->ToneSlideFreq)) {
                freq = fx->ToneSlideFreq;
                oct = fx->ToneSlideOct;
            }

        } else {

            if (oct < fx->ToneSlideOct || (oct == fx->ToneSlideOct && freq <= fx->ToneSlideFreq)) {
                freq = fx->ToneSlideFreq;
                oct = fx->ToneSlideOct;
            }
        }
    }

    chan->CurrFreq = freq;
    chan->CurrOctave = oct;

    // Apply detunes
    uint16_t frq2 = freq - chan->DetuneB;
    freq += chan->DetuneA;

    // Write value back to OPL3
    uint16_t chan_offset = Chn2Offsets3[channum];
    rad_set_opl3(rad, 0xA0 + chan_offset, freq & 0xFF);
    rad_set_opl3(rad, 0xB0 + chan_offset, (freq >> 8 & 3) | oct << 2 | (rad_get_opl3(rad, 0xB0 + chan_offset) & 0xE0));

    chan_offset = ChanOffsets3[channum];
    rad_set_opl3(rad, 0xA0 + chan_offset, frq2 & 0xFF);
    rad_set_opl3(rad, 0xB0 + chan_offset, (frq2 >> 8 & 3) | oct << 2 | (rad_get_opl3(rad, 0xB0 + chan_offset) & 0xE0));
}

//==================================================================================================
// Transpose the note returned by UnpackNote().
// Note: due to RAD's wonky legacy middle C is octave 3 note number 12.
//==================================================================================================
void rad_transpose(RADPlayer* rad, int8_t note, int8_t octave)
{

    if (rad->NoteNum >= 1 && rad->NoteNum <= 12) {

        int8_t toct = octave - 3;
        if (toct != 0) {
            rad->OctaveNum += toct;
            if (rad->OctaveNum < 0)
                rad->OctaveNum = 0;
            else if (rad->OctaveNum > 7)
                rad->OctaveNum = 7;
        }

        int8_t tnot = note - 12;
        if (tnot != 0) {
            rad->NoteNum += tnot;
            if (rad->NoteNum < 1) {
                rad->NoteNum += 12;
                if (rad->OctaveNum > 0)
                    rad->OctaveNum--;
                else
                    rad->NoteNum = 1;
            }
        }
    }
}

void rad_set_opl3(RADPlayer* rad, uint16_t reg, uint8_t val)
{
    rad->OPL3Regs[reg] = val;
    pt_sys.opl->write_reg(reg, val);
}
uint8_t rad_get_opl3(RADPlayer* rad, uint16_t reg)
{
    return rad->OPL3Regs[reg];
}

//==================================================================================================
// Compute total time of tune if it didn't repeat.  Note, this stops the tune so should only be done
// prior to initial playback.
//==================================================================================================
#if RAD_DETECT_REPEATS
static void rad_player_dummy_opl3(RADPlayer* rad, void* arg, uint16_t reg, uint8_t data)
{
}
//--------------------------------------------------------------------------------------------------
uint32_t rad_compute_total_time(RADPlayer* rad)
{

    rad_stop(rad);
    void (*old_opl3)(void*, uint16_t, uint8_t) = rad->OPL3;
    rad->OPL3 = rad_player_dummy_opl3;

    while (!rad_update(rad)) { }
    uint32_t total = rad->PlayTime;

    rad_stop(rad);
    rad->OPL3 = old_opl3;

    return total / rad->Hertz;
}
#endif

#ifdef SYSTEM_DOS
void _radplayer_lock()
{
    // Lock the memory pages that contain the RAD player
    LOCK_DATA(rad_player)
    LOCK_DATA(rad_timer)
    LOCK_DATA(NoteSize)
    LOCK_DATA(ChanOffsets3)
    LOCK_DATA(Chn2Offsets3)
    LOCK_DATA(NoteFreq)
    LOCK_DATA(OpOffsets3)
    LOCK_DATA(AlgCarriers)
}
#endif

void radplayer_init()
{
#ifdef SYSTEM_DOS
    _radplayer_lock();
#endif
}
