#ifndef PERENTIE_IMAGE_H
#define PERENTIE_IMAGE_H

#include <stdint.h>

typedef unsigned char byte;

struct image {
    byte *data;
    byte *palette;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
};

struct image *create_image(const char *path);
void destroy_image(struct image *image);

#endif
