#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "colour.h"
#include "log.h"
#include "system.h"

pt_colour_rgb ega_palette[] = {
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

static bool dither_tables_init = false;
static pt_colour_oklab ega_dither_list[256] = { 0 };

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

void rgb8_to_oklab(pt_colour_rgb* src, pt_colour_oklab* dest)
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

void oklab_to_rgb8(pt_colour_oklab* src, pt_colour_rgb* dest)
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

void oklab_colour_blend(pt_colour_oklab* a, pt_colour_oklab* b, float alpha, pt_colour_oklab* dest)
{
    dest->L = a->L * (1 - alpha) + b->L * alpha;
    dest->a = a->a * (1 - alpha) + b->a * alpha;
    dest->b = a->b * (1 - alpha) + b->b * alpha;
    return;
}

float oklab_distance(pt_colour_oklab* a, pt_colour_oklab* b)
{
    return (b->L - a->L) * (b->L - a->L) + (b->a - a->a) * (b->a - a->a) + (b->b - a->b) * (b->b - a->b);
}

void generate_dither_tables()
{
    if (dither_tables_init)
        return;

    pt_colour_oklab ega_oklab[16];
    for (int i = 0; i < 16; i++) {
        rgb8_to_oklab(&ega_palette[i], &ega_oklab[i]);
    }
    for (int i = 0; i < 256; i++) {
        oklab_colour_blend(&ega_oklab[i / 16], &ega_oklab[i % 16], 0.5f, &ega_dither_list[i]);
    }
    // erase a bunch of combinations which look bad.
    // black should only be mixed with dark-range colours
    int light[] = { 7, 9, 10, 11, 12, 13, 14, 15 };
    for (int i = 0; i < 8; i++) {
        int j = light[i];
        ega_dither_list[0 + j].L = ega_oklab[0].L;
        ega_dither_list[0 + j].a = ega_oklab[0].a;
        ega_dither_list[0 + j].b = ega_oklab[0].b;
        ega_dither_list[j * 16 + 0].L = ega_oklab[0].L;
        ega_dither_list[j * 16 + 0].a = ega_oklab[0].a;
        ega_dither_list[j * 16 + 0].b = ega_oklab[0].b;
    }

    // white should only be mixed with light-range colours
    int dark[] = { 0, 1, 2, 3, 4, 5, 6, 8 };
    for (int i = 0; i < 8; i++) {
        int j = dark[i];
        ega_dither_list[15 + j].L = ega_oklab[0].L;
        ega_dither_list[15 + j].a = ega_oklab[0].a;
        ega_dither_list[15 + j].b = ega_oklab[0].b;
        ega_dither_list[j * 16 + 15].L = ega_oklab[0].L;
        ega_dither_list[j * 16 + 15].a = ega_oklab[0].a;
        ega_dither_list[j * 16 + 15].b = ega_oklab[0].b;
    }

    dither_tables_init = true;
    return;
}

void get_ega_dither_for_color(pt_colour_rgb* src, pt_dither* dest)
{
    if (!dither_tables_init) {
        generate_dither_tables();
    }
    pt_colour_oklab src_oklab = { 0 };
    rgb8_to_oklab(src, &src_oklab);
    int nearest = 0;
    float nearest_val = oklab_distance(&src_oklab, &ega_dither_list[0]);
    for (int i = 1; i < 256; i++) {
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

void set_dither_from_remapper(enum pt_palette_remapper remapper, uint8_t idx, pt_dither* dest)
{
    switch (remapper) {
    case REMAPPER_EGA:
        get_ega_dither_for_color(&pt_sys.palette[idx], dest);
        break;
    case REMAPPER_NONE:
        // If we're explicitly setting the mapper to be NONE,
        // clear the dither table.
        dest->type = DITHER_NONE;
        dest->idx_a = 0;
        dest->idx_b = 0;
        break;
    default:
        // Don't auto change the dither.
        // Can be manually overridden.
        break;
    }
    log_print("set_dither_from_remapper: %d (%02x %02x %02x) -> %d %02x %02x\n", idx, pt_sys.palette[idx].r,
        pt_sys.palette[idx].g, pt_sys.palette[idx].b, dest->type, dest->idx_a, dest->idx_b);
}

uint8_t map_colour(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < pt_sys.palette_top; i++) {
        if (pt_sys.palette[i].r == r && pt_sys.palette[i].g == g && pt_sys.palette[i].b == b)
            return i;
    }
    // add a new colour
    if (pt_sys.palette_top < 255) {
        int idx = pt_sys.palette_top;
        pt_sys.palette_top++;
        pt_sys.palette[idx].r = r;
        pt_sys.palette[idx].g = g;
        pt_sys.palette[idx].b = b;
        set_dither_from_remapper(pt_sys.remapper, idx, &pt_sys.dither[idx]);
        pt_sys.video->update_palette_slot(idx);
        pt_sys.palette_revision++;
        return idx;
    }
    // Out of palette slots; need to macguyver the nearest colour.
    // Formula borrowed from ScummVM's palette code.
    uint8_t best_color = 0;
    uint32_t min = 0xffffffff;
    for (int i = 0; i < 255; ++i) {
        int rmean = (pt_sys.palette[i].r + r) / 2;
        int dr = pt_sys.palette[i].r - r;
        int dg = pt_sys.palette[i].g - g;
        int db = pt_sys.palette[i].b - b;

        uint32_t dist_squared = (((512 + rmean) * dr * dr) >> 8) + 4 * dg * dg + (((767 - rmean) * db * db) >> 8);
        if (dist_squared < min) {
            best_color = i;
            min = dist_squared;
        }
    }
    return best_color;
}

void dither_set_hint(pt_colour_rgb* src, enum pt_dither_type type, pt_colour_rgb* a, pt_colour_rgb* b)
{
    uint8_t idx_src = map_colour(src->r, src->g, src->b);
    uint8_t idx_a = map_colour(a->r, a->g, a->b);
    uint8_t idx_b = map_colour(b->r, b->g, b->b);
    pt_sys.dither[idx_src].type = type;
    pt_sys.dither[idx_src].idx_a = idx_a;
    pt_sys.dither[idx_src].idx_b = idx_b;
    pt_sys.palette_revision++;
}

uint8_t dither_calc(uint8_t src, int16_t x, int16_t y)
{
    pt_dither* dither = &pt_sys.dither[src];
    switch (dither->type) {
    case DITHER_D50:
        return x + y % 2 ? dither->idx_b : dither->idx_a;
    case DITHER_FILL_A:
        return dither->idx_a;
    case DITHER_FILL_B:
        return dither->idx_b;
    case DITHER_NONE:
    default:
        return src;
    }
}
