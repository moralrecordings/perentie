#ifndef PERENTIE_FONT_H
#define PERENTIE_FONT_H

#include <stdint.h>
#include <stdio.h>

#include "image.h"

// Bitmap fonts in Perentie use BMFont format.
// See http://www.angelcode.com/products/bmfont/doc/file_format.html

typedef struct pt_font_page pt_font_page;
typedef struct pt_font_common pt_font_common;
typedef struct pt_font_char pt_font_char;
typedef struct pt_font pt_font;

typedef uint8_t byte;

struct pt_font_common {
    uint16_t line_height;
    uint16_t base;
    uint16_t scale_w;
    uint16_t scale_h;
    uint16_t pages;
    uint8_t bit_field;
    uint8_t alpha_chnl;
    uint8_t red_chnl;
    uint8_t green_chnl;
    uint8_t blue_chnl;
};

struct pt_font_char {
    uint32_t id;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    int16_t xoffset;
    int16_t yoffset;
    int16_t xadvance;
    uint8_t page;
    uint8_t chnl;
};

struct pt_font {
    int32_t font_size;
    uint8_t bit_field;
    uint8_t char_set;
    uint16_t stretch_h;
    uint8_t aa;
    uint8_t padding_up;
    uint8_t padding_right;
    uint8_t padding_down;
    uint8_t padding_left;
    uint8_t spacing_horiz;
    uint8_t spacing_vert;
    uint8_t outline;
    char* font_name;

    pt_font_common common;

    pt_image** pages;
    size_t page_count;

    pt_font_char* chars;
    size_t char_count;
};

pt_font* create_font(char* path);
void destroy_font(pt_font* font);

#endif
