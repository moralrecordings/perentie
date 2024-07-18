// rough port of ScummVM's rectangle code to C
#ifndef PERENTIE_RECT_H
#define PERENTIE_RECT_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

struct rect {
    int16_t top, left;
    int16_t bottom, right;
};

inline struct rect *create_rect() {
    return (struct rect *)calloc(1, sizeof(struct rect));
}

inline struct rect *create_rect_dims(int16_t w, int16_t h) {
    struct rect *result = (struct rect *)calloc(1, sizeof(struct rect));
    result->right = w;
    result->bottom = h;
    return result;
}

inline struct rect *create_rect_bounds(int16_t t, int16_t l, int16_t b, int16_t r) {
    struct rect *result = (struct rect *)calloc(1, sizeof(struct rect));
    result->top = t;
    result->left = l;
    result->right = r;
    result->bottom = b;
    return result;
}

inline void destroy_rect(struct rect *rect) {
    if (rect)
        free(rect);
}

inline int rect_width(struct rect *rect) {
    if (!rect)
        return 0;
    return rect->right - rect->left;
}

inline int rect_height(struct rect *rect) {
    if (!rect)
        return 0;
    return rect->bottom - rect->top;
}

inline bool rect_contains_point(struct rect *rect, int16_t x, int16_t y) {
    if (!rect)
        return false;
    return (rect->left <= x) && (x < rect->right) && (rect->top <= y) && (y < rect->bottom);
}

inline bool rect_contains_rect(struct rect *rect, struct rect *target) {
    if (!rect || !target)
        return false;
    return (rect->left <= target->left) && (target->right <= rect->right) && (rect->top <= target->top) && (target->bottom <= rect->bottom);
}

inline bool rect_equals_rect(struct rect *rect, struct rect *target) {
    if (!rect || !target)
        return false;
    return (rect->left == target->left) && (rect->right == target->right) && (rect->top == target->top) && (rect->bottom == target->bottom);
}

inline bool rect_is_empty(struct rect *rect) {
    if (!rect)
        return true;
    return (rect->left >= rect->right || rect->top >= rect->bottom);
}

inline bool rect_blit_clip(int16_t *x, int16_t *y, struct rect *src, struct rect *clip) {
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
        src->right -= right - clip->right;

    int bottom = *y + src->bottom;
    if (bottom > clip->bottom)
        src->bottom -= bottom - clip->bottom;
    return !rect_is_empty(src);
}

#endif
