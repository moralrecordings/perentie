#ifndef PERENTIE_IMAGE_H
#define PERENTIE_IMAGE_H

#include <stdint.h>

typedef unsigned char byte;
typedef struct pt_image pt_image;

struct pt_image {
    const char *path;
    byte *data;
    byte *palette;
    uint16_t width;
    uint16_t height;
    int16_t origin_x;
    int16_t origin_y;
    uint16_t pitch;
};

pt_image *create_image(const char *path, int16_t origin_x, int16_t origin_y);
bool image_load(pt_image *image);
void destroy_image(pt_image *image);

#endif
