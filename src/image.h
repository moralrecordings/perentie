#ifndef PERENTIE_IMAGE_H
#define PERENTIE_IMAGE_H

#include <stdint.h>

typedef unsigned char byte;
typedef struct pt_image pt_image;

struct pt_image {
    const char *path;
    byte *data;
    byte *palette;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
};

pt_image *create_image(const char *path);
bool image_load(pt_image *image);
void destroy_image(pt_image *image);

#endif
