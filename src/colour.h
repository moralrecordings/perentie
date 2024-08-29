#ifndef PERENTIE_COLOUR_H
#define PERENTIE_COLOUR_H

#include <stdint.h>

typedef uint8_t byte;

typedef struct pt_color_rgb pt_color_rgb;
typedef struct pt_color_oklab pt_color_oklab;
typedef struct pt_dither pt_dither;

struct pt_color_rgb {
    byte r;
    byte g;
    byte b;
};

struct pt_color_oklab {
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

extern pt_color_rgb ega_palette[16];

#endif
