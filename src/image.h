#ifndef PERENTIE_IMAGE_H
#define PERENTIE_IMAGE_H

#include <stdbool.h>
#include <stdint.h>

typedef unsigned char byte;
typedef struct pt_image pt_image;

struct pt_image {
    const char* path;
    byte* data;
    byte palette[3 * 256];
    byte palette_alpha[256];
    uint16_t width;
    uint16_t height;
    int16_t origin_x;
    int16_t origin_y;
    uint16_t pitch;
    int16_t colourkey;

    void* hw_image;
};

static inline uint16_t get_pitch(uint32_t width)
{
    return (width % 4) == 0 ? width : width + 4 - (width % 4);
}

static inline int16_t image_left(pt_image* image)
{
    if (!image)
        return 0;
    return -image->origin_x;
}

static inline int16_t image_top(pt_image* image)
{
    if (!image)
        return 0;
    return -image->origin_y;
}

static inline int16_t image_right(pt_image* image)
{
    if (!image)
        return 0;
    return image->width - image->origin_x;
}

static inline int16_t image_bottom(pt_image* image)
{
    if (!image)
        return 0;
    return image->height - image->origin_y;
}

pt_image* create_image(const char* path, int16_t origin_x, int16_t origin_y, int16_t colourkey);
bool image_load(pt_image* image);
bool image_test_collision(pt_image* image, int16_t x, int16_t y, bool mask, uint8_t flags);
bool image_test_collision_9slice(pt_image* image, int16_t x, int16_t y, bool mask, uint8_t flags, uint16_t width,
    uint16_t height, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
void image_blit(pt_image* image, int16_t x, int16_t y, uint8_t flags);
void destroy_image(pt_image* image);
void image_blit_9slice(pt_image* image, int16_t x, int16_t y, uint8_t flags, uint16_t width, uint16_t height,
    int16_t x1, int16_t y1, int16_t x2, int16_t y2);

#endif
