#ifndef PERENTIE_IMAGE_H
#define PERENTIE_IMAGE_H

#include <stdint.h>

typedef unsigned char byte;
typedef struct pt_image pt_image;

struct pt_image {
    const char *path;
    byte *data;
    byte palette[3 * 256];
    uint16_t width;
    uint16_t height;
    int16_t origin_x;
    int16_t origin_y;
    uint16_t pitch;

    void *hw_image;
};

inline uint16_t get_pitch(uint32_t width) {
    return (width % 4) == 0 ? width : width + 4 - (width % 4);
}

pt_image *create_image(const char *path, int16_t origin_x, int16_t origin_y);
bool image_load(pt_image *image);
void destroy_image(pt_image *image);

#endif
