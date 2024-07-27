#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "text.h"

uint32_t iter_utf8(byte **str) {
    uint32_t result = 0;
    if (!str)
        return 0;
    byte *ptr = *str;

    if ((ptr[0] & 0x80) == 0x00) {
        result = ptr[0];
        (*str)++;
        return result;
    }
    if (!ptr[1]) {
        log_print("iter_utf8: string terminated mid-codepoint\n");
        (*str) += 2;
        return result;
    }
    if (((ptr[0] & 0xe0) == 0xc0) &&
        ((ptr[1] & 0xc0) == 0x80)) {
        result = ((ptr[0] & 0x1f) << 6) +
                    (ptr[1] & 0x3f);
        (*str) += 2;
        return result; 
    }
    if (!ptr[2]) {
        log_print("iter_utf8: string terminated mid-codepoint\n");
        (*str) += 3;
        return result;
    }
    if (((ptr[0] & 0xf0) == 0xe0) &&
        ((ptr[1] & 0xc0) == 0x80) &&
        ((ptr[2] & 0xc0) == 0x80)) {
        result = ((ptr[0] & 0x0f) << 12) +
                    ((ptr[1] & 0x3f) << 6) +
                    (ptr[2] & 0x3f);
        (*str) += 3;
        return result;
    }
    if (!ptr[3]) {
        log_print("iter_utf8: string terminated mid-codepoint\n");
        (*str) += 4;
        return result;
    }
    if (((ptr[0] & 0xf8) == 0xf0) &&
        ((ptr[1] & 0xc0) == 0x80) &&
        ((ptr[2] & 0xc0) == 0x80) &&
        ((ptr[3] & 0xc0) == 0x80)) {
        result = ((ptr[0] & 0x07) << 18) +
                    ((ptr[1] & 0x3f) << 12) +
                    ((ptr[2] & 0x3f) << 6) +
                    (ptr[3] & 0x3f);
        (*str) += 4;
        return result;
    }
    log_print("iter_utf8: invalid byte %02x\n", ptr[0]);
    (*str)++;
    return result;
}


pt_text_word *create_text_word(byte *string, size_t length, pt_font *font) {
    if (!string || !font)
        return NULL;

    pt_text_word *word = (pt_text_word *)calloc(1, sizeof(pt_text_word));
    // add bodge for outline
    word->width = font->outline;
    word->height = font->common.line_height + (font->outline * 2);
    byte *ptr = string;
    byte *end = string + length;

    while (ptr < end) {
        uint32_t codepoint = iter_utf8(&ptr);
        log_print("create_text_layout: %x\n", codepoint);
        int char_idx = -1;
        for (int i = 0; i < font->char_count; i++) {
            if (font->chars[i].id == codepoint) {
                char_idx = i;
                break;
            }
        }
        if (char_idx == -1) {
            log_print("create_text_layout: missing character for codepoint %x\n", codepoint);
            continue;
        }
        word->glyphs = (pt_text_glyph *)realloc(word->glyphs, sizeof(pt_text_glyph)*(word->glyph_count + 1));
        word->glyphs[word->glyph_count].char_idx = char_idx;
        word->glyphs[word->glyph_count].x = word->width + font->chars[char_idx].xoffset;
        word->glyphs[word->glyph_count].y = font->chars[char_idx].yoffset + font->outline;
        word->width += font->chars[char_idx].width - font->outline;
        if (!font->outline)
            word->width++;
        word->glyph_count += 1;
    }
    return word;
}

inline bool is_whitespace(byte c) {
    return (
        (c == ' ') ||
        (c == '\t') ||
        (c == '\n') ||
        (c == '\r')
    );
}

pt_text *create_text(byte *string, size_t length, pt_font *font, uint16_t width, enum pt_text_align align) {
    pt_text *text = (pt_text *)calloc(1, sizeof(pt_text)); 
    
    byte *ptr = string;
    byte *end = string + length;

    pt_text_word **words;
    size_t word_count;
    while (ptr < end) {
        byte *test = ptr;
        while ((test < end) && !is_whitespace(*test)) {
            test++;
        }
        size_t word_len = test - ptr;
        //log_print("create_text: found %d char word\n", word_len);
        pt_text_word *word = create_text_word(ptr, word_len, font);
        words = (pt_text_word **)realloc(words, sizeof(pt_text_word *)*( word_count + 1 ));
        words[word_count] = word;
        word_count++;
        ptr = test;
        while ((ptr < end) && is_whitespace(*ptr)) {
            ptr++;
        }
    }

    // Get the font-defined width of a space character.
    uint16_t space_width = 8;
    for (int i = 0; i < font->char_count; i++) {
        if (font->chars[i].id == 0x20) {
            space_width = font->chars[i].width;
            break;
        }
    }

    if (!word_count)
        return text;

    uint16_t x_cursor = 0;
    uint16_t y_cursor = 0;
    text->lines = (pt_text_line *)calloc(1, sizeof(pt_text_line));
    text->line_count = 1;
    pt_text_line *line_ptr = text->lines;
    line_ptr->y = y_cursor;
    for (int i = 0; i < word_count; i++) {
        // Words that are bigger than the bounding width should overflow.
        if (words[i]->width > text->width)
            text->width = words[i]->width;

        if (words[i]->width + x_cursor > text->width) {
            if (i > 0) {
                line_ptr->width = words[i-1]->x + words[i-1]->width;
                line_ptr->height = font->common.line_height + font->outline * 2;
            }

            x_cursor = 0;
            y_cursor += line_ptr->height;
            if (!font->outline)
                y_cursor += 2;

            text->lines = (pt_text_line *)realloc(text->lines, sizeof(pt_text_line)*(text->line_count + 1));
            line_ptr = &text->lines[text->line_count];
            line_ptr->y = y_cursor;
            text->line_count++;
        }

        words[i]->x = x_cursor;
        x_cursor += words[i]->width + space_width;
        line_ptr->words = (pt_text_word **)realloc(line_ptr->words, sizeof(pt_text_word *)*(line_ptr->word_count + 1));
        line_ptr->words[line_ptr->word_count] = words[i];
        line_ptr->word_count++;
    }
    if ((align == ALIGN_CENTER) || (align == ALIGN_RIGHT)) {
        for (int i = 0; i < text->line_count; i++) {
            pt_text_line *line = &text->lines[i];
            uint16_t nudge = align == ALIGN_CENTER ? (text->width - line->width)/2 : text->width - line->width;
            for (int j = 0; j < line->word_count; j++) {
                pt_text_word *word = line->words[j];
                word->x += nudge;
            }
        }
    }

    return text; 
}

void destroy_text(pt_text *text) {
    if (!text)
        return;
    if (text->lines) {
        for (int i = 0; i < text->line_count; i++) {
            pt_text_line *line = &text->lines[i];
            for (int j = 0; j < line->word_count; j++) {
                pt_text_word *word = line->words[j];
                if (word) {
                    if (word->glyphs) {
                        free(word->glyphs);
                        word->glyphs = NULL;
                    }
                    free(word);
                    line->words[j] = NULL;
                }
            }
            free(line->words);
            line->words = NULL;
        }
        free(text->lines);
        text->lines = NULL;
    }
    free(text);
}


