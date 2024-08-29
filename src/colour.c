#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "colour.h"

pt_color_rgb ega_palette[] = {
    { 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0xaa },
    { 0x00, 0xaa, 0x00 },
    { 0x00, 0xaa, 0xaa },
    { 0xaa, 0x00, 0x00 },
    { 0xaa, 0x00, 0xaa },
    { 0xaa, 0x55, 0x00 },
    { 0xaa, 0xaa, 0xaa },
    { 0x55, 0x55, 0x55 },
    { 0x55, 0x55, 0xff },
    { 0x55, 0xff, 0x55 },
    { 0x55, 0xff, 0xff },
    { 0xff, 0x55, 0x55 },
    { 0xff, 0x55, 0xff },
    { 0xff, 0xff, 0x55 },
    { 0xff, 0xff, 0xff },
};

float clampf(float d, float min, float max)
{
    const float t = d < min ? min : d;
    return t > max ? max : t;
}

float gamma_to_linear(float n)
{
    return n >= 0.0405f ? powf((n + 0.055f) / 1.055f, 2.4f) : n / 12.92f;
}

float linear_to_gamma(float n)
{
    return n >= 0.0031308f ? 1.055f * powf(n, 1 / 2.4f) - 0.055f : 12.92f * n;
}

void rgb8_to_oklab(pt_color_rgb* src, pt_color_oklab* dest)
{
    float t = gamma_to_linear(src->r / 255.0f);
    float i = gamma_to_linear(src->g / 255.0f);
    float r = gamma_to_linear(src->b / 255.0f);

    float u = 0.4122214708f * t + 0.5363325363f * i + 0.0514459929f * r;
    float f = 0.2119034982f * t + 0.6806995451f * i + 0.1073969566f * r;
    float e = 0.0883024619f * t + 0.2817188376f * i + 0.6299787005f * r;
    u = cbrtf(u);
    f = cbrtf(f);
    e = cbrtf(e);

    dest->L = (u * 0.2104542553f + f * 0.793617785f + e * -0.0040720468f);
    dest->a = (u * 1.9779984951f + f * -2.428592205f + e * 0.4505937099f);
    dest->b = (u * 0.0259040371f + f * 0.7827717662f + e * -0.808675766f);
    return;
}

void oklab_to_rgb8(pt_color_oklab* src, pt_color_rgb* dest)
{
    float u = src->L + src->a * 0.3963377774f + src->b * 0.2158037573f;
    float f = src->L + src->a * -0.1055613458f + src->b * -0.0638541728f;
    float e = src->L + src->a * -0.0894841775f + src->b * -1.291485548f;
    u = u * u * u;
    f = f * f * f;
    e = e * e * e;
    float i = u * 4.0767416621f + f * -3.3077115913f + e * 0.2309699292f;
    float r = u * -1.2684380046f + f * 2.6097574011f + e * -0.3413193965f;
    float t = u * -0.0041960863f + f * -0.7034186147f + e * 1.707614701f;
    dest->r = lroundf(clampf(255 * linear_to_gamma(i), 0.0f, 255.0f));
    dest->g = lroundf(clampf(255 * linear_to_gamma(r), 0.0f, 255.0f));
    dest->b = lroundf(clampf(255 * linear_to_gamma(t), 0.0f, 255.0f));
    return;
}

void oklab_colour_blend(pt_color_oklab* a, pt_color_oklab* b, float alpha, pt_color_oklab* dest)
{
    dest->L = a->L * (1 - alpha) + b->L * alpha;
    dest->a = a->a * (1 - alpha) + b->a * alpha;
    dest->b = a->b * (1 - alpha) + b->b * alpha;
    return;
}

float oklab_distance(pt_color_oklab* a, pt_color_oklab* b)
{
    return (b->L - a->L) * (b->L - a->L) + (b->a - a->a) * (b->a - a->a) + (b->b - a->b) * (b->b - a->b);
}

pt_color_oklab* generate_ega_dither_list()
{
    pt_color_oklab* ega_oklab = (pt_color_oklab*)calloc(16, sizeof(pt_color_oklab));
    for (int i = 0; i < 16; i++) {
        rgb8_to_oklab(&ega_palette[i], &ega_oklab[i]);
    }
    pt_color_oklab* ega_halftone_oklab = (pt_color_oklab*)calloc(256, sizeof(pt_color_oklab));
    for (int i = 0; i < 256; i++) {
        oklab_colour_blend(&ega_oklab[i / 16], &ega_oklab[i % 16], 0.5f, &ega_halftone_oklab[i]);
    }
    // erase a bunch of combinations which look bad.
    // black should only be mixed with dark-range colours
    int light[] = { 7, 9, 10, 11, 12, 13, 14, 15 };
    for (int i = 0; i < 8; i++) {
        int j = light[i];
        ega_halftone_oklab[0 + j].L = ega_oklab[0].L;
        ega_halftone_oklab[0 + j].a = ega_oklab[0].a;
        ega_halftone_oklab[0 + j].b = ega_oklab[0].b;
        ega_halftone_oklab[j * 16 + 0].L = ega_oklab[0].L;
        ega_halftone_oklab[j * 16 + 0].a = ega_oklab[0].a;
        ega_halftone_oklab[j * 16 + 0].b = ega_oklab[0].b;
    }

    // white should only be mixed with light-range colours
    int dark[] = { 0, 1, 2, 3, 4, 5, 6, 8 };
    for (int i = 0; i < 8; i++) {
        int j = dark[i];
        ega_halftone_oklab[15 + j].L = ega_oklab[0].L;
        ega_halftone_oklab[15 + j].a = ega_oklab[0].a;
        ega_halftone_oklab[15 + j].b = ega_oklab[0].b;
        ega_halftone_oklab[j * 16 + 15].L = ega_oklab[0].L;
        ega_halftone_oklab[j * 16 + 15].a = ega_oklab[0].a;
        ega_halftone_oklab[j * 16 + 15].b = ega_oklab[0].b;
    }

    free(ega_oklab);
    return ega_halftone_oklab;
}

void get_ega_dither_for_color(pt_color_oklab* ega_dither_list, size_t n, pt_color_rgb* src, pt_dither* dest)
{
    pt_color_oklab src_oklab = { 0 };
    rgb8_to_oklab(src, &src_oklab);
    int nearest = 0;
    float nearest_val = oklab_distance(&src_oklab, &ega_dither_list[0]);
    for (int i = 1; i < n; i++) {
        float new_val = oklab_distance(&src_oklab, &ega_dither_list[i]);
        if (new_val < nearest_val) {
            nearest_val = new_val;
            nearest = i;
        }
    }

    dest->type = DITHER_D50;
    dest->idx_a = nearest / 16;
    dest->idx_b = nearest % 16;
}
