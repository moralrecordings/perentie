#ifndef PERENTIE_TEXT_H
#define PERENTIE_TEXT_H

#include <stdbool.h>
#include <stdint.h>

#include "font.h"

typedef uint8_t byte;

typedef struct pt_text_glyph pt_text_glyph;
typedef struct pt_text_word pt_text_word;
typedef struct pt_text_line pt_text_line;
typedef struct pt_text pt_text;

struct pt_text_glyph {
    int char_idx;
    uint16_t x;
    uint16_t y;
};

struct pt_text_word {
    uint16_t x;
    uint16_t width;
    uint16_t height;
    pt_text_glyph* glyphs;
    size_t glyph_count;
};

struct pt_text_line {
    uint16_t y;
    uint16_t width;
    uint16_t height;
    pt_text_word** words;
    size_t word_count;
};

struct pt_text {
    pt_font* font; // reference, not owned
    pt_text_line** lines;
    size_t line_count;

    uint16_t width;
    uint16_t height;
};

enum pt_text_align {
    ALIGN_LEFT = 1,
    ALIGN_CENTER = 2,
    ALIGN_RIGHT = 3,
};

pt_text_word* create_text_word(const byte* string, size_t length, pt_font* font);
pt_text* create_text(const byte* string, size_t length, pt_font* font, uint16_t width, enum pt_text_align align);
pt_image* text_to_image(pt_text* text, uint8_t r, uint8_t g, uint8_t b);
void destroy_text(pt_text* text);

#endif
