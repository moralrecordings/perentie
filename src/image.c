#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spng/spng.h"

#include "image.h"
#include "log.h"
#include "system.h"

pt_image* create_image(const char* path, int16_t origin_x, int16_t origin_y, int16_t colourkey)
{
    pt_image* image = (pt_image*)calloc(1, sizeof(pt_image));
    image->path = path;
    image->origin_x = origin_x;
    image->origin_y = origin_y;
    memset(image->palette_alpha, 0xff, 256);
    image->colourkey = colourkey;
    image_load(image);
    return image;
}

bool image_load(pt_image* image)
{
    if (!image || !image->path)
        return false;

    FILE* fp = fopen(image->path, "rb");
    if (!fp) {
        log_print("image_load: Failed to open image: %s\n", image->path);
        return false;
    }

    spng_ctx* ctx = spng_ctx_new(0);
    if (!ctx) {
        log_print("image_load: Failed to create SPNG context!\n");
        fclose(fp);
        return false;
    }
    int result = 0;
    if ((result = spng_set_png_file(ctx, fp))) {
        log_print("image_load: Failed to set PNG file! %s\n", spng_strerror(result));
        spng_ctx_free(ctx);
        fclose(fp);
        return false;
    }
    struct spng_ihdr ihdr;
    if ((result = spng_get_ihdr(ctx, &ihdr))) {
        log_print("image_load: Failed to fetch IHDR! %s\n", spng_strerror(result));
        spng_ctx_free(ctx);
        fclose(fp);
        return false;
    }

    if ((ihdr.color_type != SPNG_COLOR_TYPE_INDEXED) && (ihdr.color_type != SPNG_COLOR_TYPE_GRAYSCALE)) {
        log_print("image_load: Image %s is not grayscale or paletted! %d\n", image->path, ihdr.color_type);
        spng_ctx_free(ctx);
        fclose(fp);
        return false;
    }

    log_print("image_load: path: %s\nwidth: %u\nheight: %u\nbit depth: %u\ncolor type: %u:\n", image->path, ihdr.width,
        ihdr.height, ihdr.bit_depth, ihdr.color_type);

    result = spng_decode_image(ctx, NULL, 0, SPNG_FMT_PNG, SPNG_DECODE_PROGRESSIVE);
    if (result) {
        log_print("Error decoding image: %d", result);
        spng_ctx_free(ctx);
        fclose(fp);
        return false;
    }

    image->width = (int16_t)ihdr.width;
    image->height = (int16_t)ihdr.height;
    image->pitch = get_pitch(ihdr.width);
    image->data = (byte*)calloc(image->height * image->pitch, sizeof(byte));

    if (ihdr.color_type == SPNG_COLOR_TYPE_INDEXED) {
        struct spng_plte pal;
        spng_get_plte(ctx, &pal);
        for (size_t i = 0; i < pal.n_entries; i++) {
            image->palette[3 * i] = pal.entries[i].red;
            image->palette[3 * i + 1] = pal.entries[i].green;
            image->palette[3 * i + 2] = pal.entries[i].blue;
        }
        struct spng_trns trns;
        int result = spng_get_trns(ctx, &trns);
        if (result == 0) {
            // log_print("trns: %d\n", result);
            for (size_t i = 0; i < (trns.n_type3_entries > 256 ? 256 : trns.n_type3_entries); i++) {
                // log_print("%d: %02x\n", i, trns.type3_alpha[i]);
                image->palette_alpha[i] = trns.type3_alpha[i];
            }
        }
    } else if (ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE) {
        switch (ihdr.bit_depth) {
        case 8:
            for (size_t i = 0; i < 256; i++) {
                image->palette[3 * i] = i;
                image->palette[3 * i + 1] = i;
                image->palette[3 * i + 2] = i;
            }
            break;
        case 4:
            for (size_t i = 0; i < 16; i++) {
                image->palette[3 * i] = i + (i << 4);
                image->palette[3 * i + 1] = i + (i << 4);
                image->palette[3 * i + 2] = i + (i << 4);
            }
            break;
        case 2:
            image->palette[0] = 0x00;
            image->palette[1] = 0x00;
            image->palette[2] = 0x00;
            image->palette[3] = 0x55;
            image->palette[4] = 0x55;
            image->palette[5] = 0x55;
            image->palette[6] = 0xaa;
            image->palette[7] = 0xaa;
            image->palette[8] = 0xaa;
            image->palette[9] = 0xff;
            image->palette[10] = 0xff;
            image->palette[11] = 0xff;
            break;
        case 1:
            image->palette[0] = 0x00;
            image->palette[1] = 0x00;
            image->palette[2] = 0x00;
            image->palette[3] = 0xff;
            image->palette[4] = 0xff;
            image->palette[5] = 0xff;
            break;
        default:
            break;
        }

        struct spng_trns trns;
        int result = spng_get_trns(ctx, &trns);
        if (result == 0) {
            if (trns.gray < 256) {
                image->palette_alpha[trns.gray] = 0x00;
            }
        }
    }

    byte* row_buffer = (byte*)calloc(image->width, sizeof(byte));

    struct spng_row_info row_info;
    do {
        result = spng_get_row_info(ctx, &row_info);
        if (result)
            break;
        byte* row = image->data + (row_info.row_num * image->pitch);
        result = spng_decode_row(ctx, row_buffer, image->width);
        switch (ihdr.bit_depth) {
        case 8:
            memcpy(row, row_buffer, image->width);
            break;
        case 4:
            for (int i = 0; i < image->width; i += 2) {
                byte datum = row_buffer[(i >> 1)];
                row[i] = (datum & 0xf0) >> 4;
                row[i + 1] = (datum & 0xf);
            }
            break;
        case 2:
            for (int i = 0; i < image->width; i += 4) {
                byte datum = row_buffer[(i >> 2)];
                row[i] = (datum & 0xc0) >> 6;
                row[i + 1] = (datum & 0x30) >> 4;
                row[i + 2] = (datum & 0xc) >> 2;
                row[i + 3] = (datum & 0x3);
            }
            break;
        case 1:
            for (int i = 0; i < image->width; i += 8) {
                byte datum = row_buffer[(i >> 3)];
                row[i] = (datum & 0x80) >> 7;
                row[i + 1] = (datum & 0x40) >> 6;
                row[i + 2] = (datum & 0x20) >> 5;
                row[i + 3] = (datum & 0x10) >> 4;
                row[i + 4] = (datum & 0x8) >> 3;
                row[i + 5] = (datum & 0x4) >> 2;
                row[i + 6] = (datum & 0x2) >> 1;
                row[i + 7] = (datum & 0x1);
            }
            break;
        default:
            log_print("image_load: How the hell do you have a %d bit image", ihdr.bit_depth);
            break;
        }
    } while (!result);
    free(row_buffer);
    if (result != SPNG_EOI) {
        log_print("image_load: Expected EOI, got %d", result);
        spng_ctx_free(ctx);
        fclose(fp);
        return false;
    }
    spng_ctx_free(ctx);
    fclose(fp);

    return true;
}

bool image_test_collision(pt_image* image, int16_t x, int16_t y, bool mask, uint8_t flags)
{
    if (!image)
        return false;
    // bounding box check
    if (x < image_left(image) || x >= image_right(image) || y < image_top(image) || y >= image_bottom(image))
        return false;
    // pixel check
    if (mask) {
        byte* ptr = image->data;
        ptr += image->pitch * ((flags & FLIP_V) ? (image_bottom(image) - 1 - y) : (y - image_top(image)));
        ptr += (flags & FLIP_H) ? (image_right(image) - 1 - x) : (x - image_left(image));
        if ((*ptr == image->colourkey) || (image->palette_alpha[*ptr] == 0x00))
            return false;
    }
    return true;
}

void destroy_image(pt_image* image)
{
    if (!image) {
        return;
    }
    if (image->data) {
        free(image->data);
        image->data = NULL;
    }
    if (image->hw_image) {
        pt_sys.video->destroy_hw_image(image->hw_image);
        image->hw_image = NULL;
    }
    free(image);
}
