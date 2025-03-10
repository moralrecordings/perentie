// rough port of ScummVM's point code to C
#ifndef PERENTIE_POINT_H
#define PERENTIE_POINT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct point {
    int16_t x;
    int16_t y;
};

static inline bool point_equal(struct point a, struct point b)
{
    return (a.x == b.x) && (a.y == b.y);
}

static inline struct point point_add(struct point a, struct point b)
{
    struct point result { (int16_t)(a.x + b.x), (int16_t)(a.y + b.y) };
    return result;
}

static inline struct point point_sub(struct point a, struct point b)
{
    struct point result { (int16_t)(a.x - b.x), (int16_t)(a.y - b.y) };
    return result;
}

static inline struct point point_div(struct point a, struct point b)
{
    struct point result { (int16_t)(a.x / b.x), (int16_t)(a.y / b.y) };
    return result;
}

static inline struct point point_mul(struct point a, struct point b)
{
    struct point result { (int16_t)(a.x * b.x), (int16_t)(a.y * b.y) };
    return result;
}

#endif
