#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "rect.h"
#include "text.h"

uint32_t iter_utf8(const byte** str)
{
    uint32_t result = 0;
    if (!str)
        return 0;
    const byte* ptr = *str;

    // UTF-8 1 byte codepoint - U+0000 to U+007F
    // 0xxxxxxx
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
    // UTF-8 2 byte codepoint - U+0080 to U+07FF
    // 110xxxxx 10xxxxxx
    if (((ptr[0] & 0xe0) == 0xc0) && ((ptr[1] & 0xc0) == 0x80)) {
        result = ((ptr[0] & 0x1f) << 6) + (ptr[1] & 0x3f);
        if (!(result & 0x780)) {
            log_print("iter_utf8: invalid 2-byte codepoint value %d\n", result);
            result = 0;
        }
        (*str) += 2;
        return result;
    }
    if (!ptr[2]) {
        log_print("iter_utf8: string terminated mid-codepoint\n");
        (*str) += 3;
        return result;
    }
    // UTF-8 3 byte codepoint - U+0800 to U+FFFF
    // 1110xxxx 10xxxxxx 10xxxxxx
    if (((ptr[0] & 0xf0) == 0xe0) && ((ptr[1] & 0xc0) == 0x80) && ((ptr[2] & 0xc0) == 0x80)) {
        result = ((ptr[0] & 0x0f) << 12) + ((ptr[1] & 0x3f) << 6) + (ptr[2] & 0x3f);
        if (!(result & 0xf800)) {
            log_print("iter_utf8: invalid 3-byte codepoint value %d\n", result);
            result = 0;
        }
        (*str) += 3;
        return result;
    }
    if (!ptr[3]) {
        log_print("iter_utf8: string terminated mid-codepoint\n");
        (*str) += 4;
        return result;
    }
    // UTF-8 4 byte codepoint - U+010000 to U+10FFFF
    // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    if (((ptr[0] & 0xf8) == 0xf0) && ((ptr[1] & 0xc0) == 0x80) && ((ptr[2] & 0xc0) == 0x80)
        && ((ptr[3] & 0xc0) == 0x80)) {
        result = ((ptr[0] & 0x07) << 18) + ((ptr[1] & 0x3f) << 12) + ((ptr[2] & 0x3f) << 6) + (ptr[3] & 0x3f);
        if (!(result & 0x1f0000)) {
            log_print("iter_utf8: invalid 4-byte codepoint value %d\n", result);
            result = 0;
        }
        (*str) += 4;
        return result;
    }
    log_print("iter_utf8: invalid byte %02x\n", ptr[0]);
    (*str)++;
    return result;
}

pt_text_word* create_newline()
{
    pt_text_word* word = (pt_text_word*)calloc(1, sizeof(pt_text_word));
    word->newline = true;
    return word;
}

pt_text_word* create_text_word(const byte* string, size_t length, pt_font* font)
{
    if (!string || !font)
        return NULL;

    pt_text_word* word = (pt_text_word*)calloc(1, sizeof(pt_text_word));
    // add bodge for outline
    word->width = font->outline;
    word->height = font->common.line_height + (font->outline * 2);
    word->newline = false;
    const byte* ptr = string;
    const byte* end = string + length;

    while (ptr < end) {
        uint32_t codepoint = iter_utf8(&ptr);
        // log_print("create_text_word: %x\n", codepoint);
        int char_idx = -1;
        for (int i = 0; i < font->char_count; i++) {
            if (font->chars[i].id == codepoint) {
                char_idx = i;
                break;
            }
        }
        if (char_idx == -1) {
            log_print("create_text_word: missing character for codepoint %x\n", codepoint);
            continue;
        }
        word->glyphs = (pt_text_glyph*)realloc(word->glyphs, sizeof(pt_text_glyph) * (word->glyph_count + 1));
        word->glyphs[word->glyph_count].char_idx = char_idx;
        word->glyphs[word->glyph_count].x = word->width;
        word->glyphs[word->glyph_count].y = font->chars[char_idx].yoffset + font->outline;
        word->width += font->chars[char_idx].width - font->outline;
        if (!font->outline)
            word->width++;
        word->glyph_count += 1;
    }
    return word;
}

static inline bool is_whitespace(byte c)
{
    return ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r'));
}

static inline bool is_whitespace_except_newline(byte c)
{
    return ((c == ' ') || (c == '\t') || (c == '\r'));
}

pt_text* create_text(const byte* string, size_t length, pt_font* font, uint16_t width, enum pt_text_align align)
{
    pt_text* text = (pt_text*)calloc(1, sizeof(pt_text));
    text->font = font;
    text->width = width;
    if (length == 0)
        return text;

    const byte* ptr = string;
    const byte* end = string + length;

    pt_text_word** words = NULL;
    size_t word_count = 0;
    while (ptr < end) {
        if (*ptr == '\n') {
            pt_text_word* word = create_newline();
            words = (pt_text_word**)realloc(words, sizeof(pt_text_word*) * (word_count + 1));
            words[word_count] = word;
            word_count++;
            ptr++;
            continue;
        }
        const byte* test = ptr;
        while ((test < end) && !is_whitespace(*test)) {
            test++;
        }
        size_t word_len = test - ptr;
        // log_print("create_text: found %d char word\n", word_len);
        pt_text_word* word = create_text_word(ptr, word_len, font);
        // log_print("create_text: word size: %dx%d\n", word->width, word->height);
        words = (pt_text_word**)realloc(words, sizeof(pt_text_word*) * (word_count + 1));
        words[word_count] = word;
        word_count++;
        ptr = test;
        while ((ptr < end) && is_whitespace_except_newline(*ptr)) {
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

    if (!word_count) {
        if (words) {
            for (int j = 0; j < word_count; j++) {
                destroy_text_word(words[j]);
            }
            free(words);
        }
        return text;
    }

    uint16_t x_cursor = 0;
    uint16_t y_cursor = 0;
    text->lines = (pt_text_line**)calloc(1, sizeof(pt_text_line*));
    text->line_count = 1;
    text->lines[0] = (pt_text_line*)calloc(1, sizeof(pt_text_line));
    pt_text_line* line_ptr = text->lines[0];
    line_ptr->y = y_cursor;
    line_ptr->height = font->common.line_height + font->outline * 2;
    for (int i = 0; i < word_count; i++) {
        // Words that are bigger than the bounding width should overflow.
        if (words[i]->width > text->width) {
            // log_print("create_text: resizing width from %d to %d\n", text->width, words[i]->width);
            text->width = words[i]->width;
        }

        // If we run out of horizontal space, or hit a newline
        if (words[i]->newline || (words[i]->width + x_cursor > text->width)) {

            // For words other than the first word, set the final dims of the line.
            if (line_ptr->word_count > 0) {
                pt_text_word* last_word = line_ptr->words[line_ptr->word_count - 1];
                line_ptr->width = last_word->x + last_word->width;
            }

            x_cursor = 0;
            y_cursor += line_ptr->height;
            if (!font->outline)
                y_cursor += 2;

            // resize the lines list, create a new line
            text->lines = (pt_text_line**)realloc(text->lines, sizeof(pt_text_line*) * (text->line_count + 1));
            text->lines[text->line_count] = (pt_text_line*)calloc(1, sizeof(pt_text_line));
            line_ptr = text->lines[text->line_count];
            line_ptr->y = y_cursor;
            line_ptr->height = font->common.line_height + font->outline * 2;
            text->line_count++;
        }

        // Newlines don't count as a word.
        // We don't transfer ownership, so destroy it here.
        if (words[i]->newline) {
            destroy_text_word(words[i]);
            words[i] = NULL;
            continue;
        }

        words[i]->x = x_cursor;
        x_cursor += words[i]->width + space_width;
        line_ptr->words = (pt_text_word**)realloc(line_ptr->words, sizeof(pt_text_word*) * (line_ptr->word_count + 1));
        line_ptr->words[line_ptr->word_count] = words[i];
        line_ptr->word_count++;
    }
    if (line_ptr->word_count > 0) {
        pt_text_word* last_word = line_ptr->words[line_ptr->word_count - 1];
        line_ptr->width = last_word->x + last_word->width;
    }

    // log_print("create_text: resizing height from %d to %d + %d\n", text->height, line_ptr->y, line_ptr->height);
    text->height = line_ptr->y + line_ptr->height;

    if ((align == ALIGN_CENTER) || (align == ALIGN_RIGHT)) {
        for (int i = 0; i < text->line_count; i++) {
            pt_text_line* line = text->lines[i];
            uint16_t nudge = align == ALIGN_CENTER ? (text->width - line->width) / 2 : text->width - line->width - 1;
            // log_print("create_text: nudging line %d (width %d) by %d px\n", i, line->width, nudge);
            for (int j = 0; j < line->word_count; j++) {
                line->words[j]->x += nudge;
            }
        }
    }
    free(words);
    return text;
}

pt_image* text_to_image(pt_text* text, uint8_t r, uint8_t g, uint8_t b, uint8_t brd_r, uint8_t brd_g, uint8_t brd_b)
{
    if (!text)
        return NULL;
    pt_image* image = create_image(NULL, 0, 0, 0);
    if (!image)
        return NULL;
    image->width = text->width;
    image->height = text->height;
    image->pitch = get_pitch(text->width);
    image->data = (byte*)calloc(image->pitch * image->height, sizeof(byte));
    image->palette[0x7f * 3] = brd_r;
    image->palette[0x7f * 3 + 1] = brd_g;
    image->palette[0x7f * 3 + 2] = brd_b;
    image->palette[0xff * 3] = r;
    image->palette[0xff * 3 + 1] = g;
    image->palette[0xff * 3 + 2] = b;
    // log_print("text_to_image: creating %dx%d bitmap (%d bytes)\n", image->width, image->height, image->pitch *
    // image->height);

    struct rect* char_rect = create_rect();
    struct rect* crop = create_rect_dims(image->width, image->height);

    for (int i = 0; i < text->line_count; i++) {
        pt_text_line* line = text->lines[i];
        // log_print("text_to_image: line %d, ypos=%d, %dx%d\n", i, line->y, line->width, line->height);
        for (int j = 0; j < line->word_count; j++) {
            pt_text_word* word = line->words[j];
            // log_print("text_to_image: word %d, xpos=%d, %dx%d\n", j, word->x, word->width, word->height);
            for (int k = 0; k < word->glyph_count; k++) {
                pt_text_glyph* glyph = &word->glyphs[k];
                // log_print("font %d %d %p\n", k, glyph->char_idx);
                pt_font_char* fchar = &text->font->chars[glyph->char_idx];
                pt_image* page = text->font->pages[fchar->page];
                if (!page->data) {
                    log_print("text_to_image: page %d missing\n", fchar->page);
                    break;
                }

                int16_t x = word->x + glyph->x;
                int16_t y = line->y + glyph->y;
                char_rect->left = 0;
                char_rect->top = 0;
                char_rect->right = fchar->width;
                char_rect->bottom = fchar->height;
                if (!rect_blit_clip(&x, &y, char_rect, crop)) {
                    log_print("text_to_image: We messed up: U+%04X %d,%d %dx%d doesn't fit in %dx%d\n", fchar->id, x, y,
                        fchar->width, fchar->height, image->width, image->height);
                    continue;
                }
                // log_print("text_to_image: char_rect: %d %d %d %d\n", char_rect->left, char_rect->top,
                // char_rect->right, char_rect->bottom);

                int16_t x_start = x;
                // BMFont will produce greyscale PNG images; colours are one of:
                // 0x00 (mask), 0x7f (outline) or 0xff (body).
                // Using bitwise OR means we can merge adjacent characters together
                // (e.g. two letters with a shared outline pixel)
                for (int yi = char_rect->top; yi < char_rect->bottom; yi++) {
                    for (int xi = char_rect->left; xi < char_rect->right; xi++) {
                        // log_print("text_to_image: source (%d, %d) -> %d\n", (xi + fchar->x), (yi + fchar->y),
                        // page->pitch * (yi + fchar->y) + (xi + fchar->x));
                        byte src = page->data[page->pitch * (yi + fchar->y) + (xi + fchar->x)];
                        // log_print("text_to_image: target (%d, %d) -> %d\n", x, y, image->pitch * y + x);
                        image->data[image->pitch * (y) + x] |= src;
                        x++;
                    }
                    y++;
                    x = x_start;
                }
            }
        }
    }
    destroy_rect(char_rect);
    destroy_rect(crop);
    return image;
}

void destroy_text_word(pt_text_word* word)
{
    if (!word)
        return;
    if (word->glyphs) {
        free(word->glyphs);
        word->glyphs = NULL;
    }
    free(word);
}

void destroy_text(pt_text* text)
{
    if (!text)
        return;
    if (text->lines) {
        for (int i = 0; i < text->line_count; i++) {
            pt_text_line* line = text->lines[i];
            for (int j = 0; j < line->word_count; j++) {
                destroy_text_word(line->words[j]);
                line->words[j] = NULL;
            }
            free(line->words);
            line->words = NULL;
            free(line);
            text->lines[i] = NULL;
        }
        free(text->lines);
        text->lines = NULL;
    }
    free(text);
}
