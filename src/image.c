#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "spng/spng.h"

#include "image.h"

uint8_t get_shift(uint32_t width) {
    for (uint8_t i = 0; i < 32; i++) {
        if ((1 << i) >= width) {
            return i;
        }
    }
}


struct image *create_image(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        printf("Failed to open image: %s", path);
        return NULL;
    }

    spng_ctx *ctx = spng_ctx_new(0);
    if (!ctx) {
        printf("Failed to create SPNG context!");
        fclose(fp);
        return NULL;
    }
    int result = 0;
    if ((result = spng_set_png_file(ctx, fp))) {
        printf("Failed to set PNG file! %s", spng_strerror(result));
        spng_ctx_free(ctx);
        fclose(fp);
        return NULL;
    }
    struct spng_ihdr ihdr;
    if ((result = spng_get_ihdr(ctx, &ihdr))) {
        printf("Failed to fetch IHDR! %s", spng_strerror(result));
        spng_ctx_free(ctx);
        fclose(fp);
        return NULL;
    }    
    
    if (ihdr.color_type != SPNG_COLOR_TYPE_INDEXED) {
        printf("Image %s is not paletted! %d", path, ihdr.color_type);
        spng_ctx_free(ctx);
        fclose(fp);
        return NULL;
    }

    printf("width: %u\nheight: %u\nbit depth: %u\ncolor type: %u:\n",
           ihdr.width, ihdr.height, ihdr.bit_depth, ihdr.color_type);

    result = spng_decode_image(ctx, NULL, 0, SPNG_FMT_PNG, SPNG_DECODE_PROGRESSIVE);
    if (result) {
        printf("Error decoding image: %d", result);
        spng_ctx_free(ctx);
        fclose(fp);
        return NULL;
    }

    struct image *image = (struct image *)calloc(1, sizeof(struct image));
    image->width = ihdr.width;
    image->height = ihdr.height; 
    image->shift = get_shift(ihdr.width);
    image->data = (byte *)calloc(image->height << image->shift, sizeof(byte));
    image->palette = (byte *)calloc(256*3, sizeof(byte));

    byte *row_buffer = (byte *)calloc(image->width, sizeof(byte));

    struct spng_row_info row_info;
    do {
        result = spng_get_row_info(ctx, &row_info);
        if (result) break;
        byte *row = image->data + (row_info.row_num << image->shift);
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
                printf("How the hell do you have a %d bit image", ihdr.bit_depth);
                break;
        }
    }
    while (!result);
    free(row_buffer);
    if (result != SPNG_EOI) {
        printf("Expected EOI, got %d", result);
        spng_ctx_free(ctx);
        fclose(fp);
        return NULL;
    }
    spng_ctx_free(ctx);
    fclose(fp);

    return image;
}

void destroy_image(struct image *image) {
    if (!image) {
        return;
    }
    if (image->data)
        free(image->data);
    if (image->palette)
        free(image->palette);
    free(image);
}
