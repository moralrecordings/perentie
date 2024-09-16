#ifndef PERENTIE_COLOUR_H
#define PERENTIE_COLOUR_H

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
    DITHER_D50,
    DITHER_FILL_B,
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

extern pt_colour_rgb ega_palette[16];

pt_colour_oklab* generate_ega_dither_list();
void get_ega_dither_for_color(pt_colour_oklab* ega_dither_list, size_t n, pt_colour_rgb* src, pt_dither* dest);

#endif
