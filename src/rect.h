// rough port of ScummVM's rectangle code to C
#ifndef PERENTIE_RECT_H
#define PERENTIE_RECT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct rect {
    int16_t left, top;
    int16_t right, bottom;
};

inline struct rect* create_rect()
{
    return (struct rect*)calloc(1, sizeof(struct rect));
}

inline struct rect* create_rect_dims(int16_t w, int16_t h)
{
    struct rect* result = (struct rect*)calloc(1, sizeof(struct rect));
    result->right = w;
    result->bottom = h;
    return result;
}

inline struct rect* create_rect_bounds(int16_t t, int16_t l, int16_t b, int16_t r)
{
    struct rect* result = (struct rect*)calloc(1, sizeof(struct rect));
    result->left = l;
    result->top = t;
    result->right = r;
    result->bottom = b;
    return result;
}

inline void destroy_rect(struct rect* rect)
{
    if (rect)
        free(rect);
}

inline int rect_width(struct rect* rect)
{
    if (!rect)
        return 0;
    return rect->right - rect->left;
}

inline int rect_height(struct rect* rect)
{
    if (!rect)
        return 0;
    return rect->bottom - rect->top;
}

inline bool rect_contains_point(struct rect* rect, int16_t x, int16_t y)
{
    if (!rect)
        return false;
    return (rect->left <= x) && (x < rect->right) && (rect->top <= y) && (y < rect->bottom);
}

inline bool rect_contains_rect(struct rect* rect, struct rect* target)
{
    if (!rect || !target)
        return false;
    return (rect->left <= target->left) && (target->right <= rect->right) && (rect->top <= target->top)
        && (target->bottom <= rect->bottom);
}

inline bool rect_equals_rect(struct rect* rect, struct rect* target)
{
    if (!rect || !target)
        return false;
    return (rect->left == target->left) && (rect->right == target->right) && (rect->top == target->top)
        && (rect->bottom == target->bottom);
}

inline bool rect_is_empty(struct rect* rect)
{
    if (!rect)
        return true;
    return (rect->left >= rect->right || rect->top >= rect->bottom);
}

inline void rect_move_to(struct rect* rect, int16_t x, int16_t y)
{
    if (!rect)
        return;
    rect->bottom += y - rect->top;
    rect->right += x - rect->left;
    rect->top = y;
    rect->left = x;
}

inline void rect_translate(struct rect* rect, int16_t dx, int16_t dy)
{
    if (!rect)
        return;
    rect->left += dx;
    rect->right += dx;
    rect->top += dy;
    rect->bottom += dy;
}

inline bool rect_blit_clip(int16_t* x, int16_t* y, struct rect* src, struct rect* clip)
{
    if (!x || !y || !src || !clip)
        return false;
    if (*x < clip->left) {
        src->left += clip->left - *x;
        *x = clip->left;
    }
    if (*y < clip->top) {
        src->top += clip->top - *y;
        *y = clip->top;
    }
    int right = *x + src->right;
    if (right > clip->right)
        src->right = clip->right + src->left - *x;

    int bottom = *y + src->bottom;
    if (bottom > clip->bottom)
        src->bottom = clip->bottom + src->top - *y;
    return !rect_is_empty(src);
}

#endif
