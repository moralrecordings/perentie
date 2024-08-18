#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dos.h"
#include "font.h"
#include "log.h"
#include "utils.h"

void font_load_info_block(FILE* fp, size_t size, pt_font* font)
{
    if (size < 15) {
        log_print("font_load_info_block: %d is too small\n", size);
        fseek(fp, size, SEEK_CUR);
        return;
    }
    font->font_size = fread_i16le(fp);
    font->bit_field = fread_u8(fp);
    font->char_set = fread_u8(fp);
    font->stretch_h = fread_u16le(fp);
    font->aa = fread_u8(fp);
    font->padding_up = fread_u8(fp);
    font->padding_right = fread_u8(fp);
    font->padding_down = fread_u8(fp);
    font->padding_left = fread_u8(fp);
    font->spacing_horiz = fread_u8(fp);
    font->spacing_vert = fread_u8(fp);
    font->outline = fread_u8(fp);
    size -= 14;
    font->font_name = (char*)calloc(size, sizeof(char));
    fread(font->font_name, sizeof(char), size, fp);
}

void font_load_common_block(FILE* fp, size_t size, pt_font* font)
{
    if (size < 15) {
        log_print("font_load_common_block: %d is too small\n", size);
        fseek(fp, size, SEEK_CUR);
        return;
    }
    font->common.line_height = fread_u16le(fp);
    font->common.base = fread_u16le(fp);
    font->common.scale_w = fread_u16le(fp);
    font->common.scale_h = fread_u16le(fp);
    font->common.pages = fread_u16le(fp);
    font->common.bit_field = fread_u8(fp);
    font->common.alpha_chnl = fread_u8(fp);
    font->common.red_chnl = fread_u8(fp);
    font->common.green_chnl = fread_u8(fp);
    font->common.blue_chnl = fread_u8(fp);
    size -= 15;
    fseek(fp, size, SEEK_CUR);
}

void font_load_pages_block(FILE* fp, size_t size, pt_font* font, const char* path)
{
    char buffer[256];
    char* ptr = buffer;

    char* dir_sep = strrchr(path, '/');
    size_t dir_size = dir_sep ? dir_sep - path + 1 : 0;

    memset(buffer, 0, 256);
    while (size && ptr < buffer + 255) {
        *ptr = fgetc(fp);
        size--;
        if (*ptr == '\0') {
            font->pages = realloc(font->pages, sizeof(pt_image*) * (font->page_count + 1));
            size_t name_size = dir_size + strlen(buffer) + 1;
            char* atlas_path = (char*)calloc(name_size, sizeof(char));
            // Prepend the directory of the font data to the atlas image
            memcpy(atlas_path, path, dir_size);
            memcpy(atlas_path + dir_size, buffer, strlen(buffer));
            font->pages[font->page_count] = create_image(atlas_path, 0, 0, 0);
            font->page_count++;
            memset(buffer, 0, 256);
            ptr = buffer;
        } else {
            ptr++;
        }
    }
    if (size) {
        log_print("font_load_pages_block: %d extra bytes");
        fseek(fp, size, SEEK_CUR);
    }
}

void font_load_chars_block(FILE* fp, size_t size, pt_font* font)
{
    if (size % 20 != 0) {
        log_print("font_load_chars_block: %d is not divisible by 20\n", size);
        fseek(fp, size, SEEK_CUR);
    }
    font->char_count = size / 20;
    font->chars = calloc(font->char_count, sizeof(pt_font_char));
    for (int i = 0; i < font->char_count; i++) {
        font->chars[i].id = fread_u32le(fp);
        font->chars[i].x = fread_u16le(fp);
        font->chars[i].y = fread_u16le(fp);
        font->chars[i].width = fread_u16le(fp);
        font->chars[i].height = fread_u16le(fp);
        font->chars[i].xoffset = fread_i16le(fp);
        font->chars[i].yoffset = fread_i16le(fp);
        font->chars[i].xadvance = fread_i16le(fp);
        font->chars[i].page = fread_u8(fp);
        font->chars[i].chnl = fread_u8(fp);
        // log_print("font_load_chars_block: id=%d, x=%d, y=%d, width=%d, height=%d\n", font->chars[i].id,
        //      font->chars[i].x, font->chars[i].y, font->chars[i].width, font->chars[i].height);
    }
}

void font_load_kerning_block(FILE* fp, size_t size, pt_font* font)
{
    log_print("font_load_kerning_block: not implemented\n");
    fseek(fp, size, SEEK_CUR);
}

pt_font* create_font(const char* path)
{
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        log_print("create_font: Unable to open %s\n", path);
        return NULL;
    }

    uint32_t magic = fread_u32be(fp);
    if (magic != 0x424d4603) { // "BMF"
        log_print("create_font: Only BMFont V3 binary format is supported, not found %s\n", path);
        fclose(fp);
        return NULL;
    }
    pt_font* font = (pt_font*)calloc(1, sizeof(pt_font));

    bool done = false;
    while (!done || !feof(fp)) {
        uint8_t type = fread_u8(fp);
        size_t size = fread_u32le(fp);
        switch (type) {
        case 1:
            log_print("create_font: found info block, %d bytes\n", size);
            font_load_info_block(fp, size, font);
            break;
        case 2:
            log_print("create_font: found common block, %d bytes\n", size);
            font_load_common_block(fp, size, font);
            break;
        case 3:
            log_print("create_font: found pages block, %d bytes\n", size);
            font_load_pages_block(fp, size, font, path);
            break;
        case 4:
            log_print("create_font: found chars block, %d bytes\n", size);
            font_load_chars_block(fp, size, font);
            break;
        case 5:
            log_print("create_font: found kerning block, %d bytes\n", size);
            font_load_kerning_block(fp, size, font);
            break;
        default:
            log_print("create_font: unknown data, stopping at %04x\n", ftell(fp));
            done = true;
            break;
        }
    }
    log_print(
        "create_font: Loaded \"%s\", %d pages, %d characters\n", font->font_name, font->page_count, font->char_count);
    return font;
}

void destroy_font(pt_font* font)
{
    if (!font)
        return;
    if (font->font_name) {
        free(font->font_name);
        font->font_name = NULL;
    }
    if (font->pages) {
        for (int i = 0; i < font->page_count; i++) {
            destroy_image(font->pages[i]);
            font->pages[i] = NULL;
        }
        free(font->pages);
        font->pages = NULL;
    }
    if (font->chars) {
        free(font->chars);
        font->chars = NULL;
    }
    free(font);
}
