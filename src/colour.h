#ifndef PERENTIE_COLOUR_H
#define PERENTIE_COLOUR_H

#include <stddef.h>
#include <stdint.h>

typedef uint8_t byte;

typedef struct pt_colour_rgb pt_colour_rgb;
typedef struct pt_colour_oklab pt_colour_oklab;
typedef struct pt_dither pt_dither;

struct pt_colour_rgb {
    byte r;
    byte g;
    byte b;
};

struct pt_colour_oklab {
    float L;
    float a;
    float b;
};

enum pt_dither_type {
    DITHER_NONE = 0,
    DITHER_FILL_A,
    DITHER_FILL_B,
    DITHER_QUARTER,
    DITHER_QUARTER_ALT,
    DITHER_HALF,
};

struct pt_dither {
    enum pt_dither_type type;
    uint8_t idx_a;
    uint8_t idx_b;
};

enum pt_palette_remapper {
    REMAPPER_NONE = 0,
    REMAPPER_EGA = 1,
    REMAPPER_CGA0A = 2,
    REMAPPER_CGA0B = 3,
    REMAPPER_CGA1A = 4,
    REMAPPER_CGA1B = 5,
    REMAPPER_CGA2A = 6,
    REMAPPER_CGA2B = 7,
};

enum pt_palette_remapper_mode {
    REMAPPER_MODE_NEAREST = 0,
    REMAPPER_MODE_HALF = 1,
    REMAPPER_MODE_QUARTER = 2,
    REMAPPER_MODE_QUARTER_ALT = 3,
    REMAPPER_MODE_HALF_NEAREST = 4,
    REMAPPER_MODE_QUARTER_NEAREST = 5,
};

enum pt_ega {
    EGA_BLACK = 0,
    EGA_BLUE = 1,
    EGA_GREEN = 2,
    EGA_CYAN = 3,
    EGA_RED = 4,
    EGA_MAGENTA = 5,
    EGA_BROWN = 6,
    EGA_LGRAY = 7,
    EGA_DGRAY = 8,
    EGA_BRBLUE = 9,
    EGA_BRGREEN = 10,
    EGA_BRCYAN = 11,
    EGA_BRRED = 12,
    EGA_BRMAGENTA = 13,
    EGA_BRYELLOW = 14,
    EGA_WHITE = 15
};

extern pt_colour_rgb ega_palette[16];

void set_dither_from_remapper(
    enum pt_palette_remapper remapper, enum pt_palette_remapper_mode mode, uint8_t idx, pt_dither* dest);
void get_ega_dither_for_colour(enum pt_palette_remapper_mode mode, pt_colour_rgb* src, pt_dither* dest);
uint8_t map_colour(uint8_t r, uint8_t g, uint8_t b);
void dither_set_hint(pt_colour_rgb* src, enum pt_dither_type type, pt_colour_rgb* a, pt_colour_rgb* b);
uint8_t dither_calc(uint8_t src, int16_t x, int16_t y);

#endif
