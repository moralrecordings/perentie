#ifndef PERENTIE_EVENT_H
#define PERENTIE_EVENT_H

#include <stdbool.h>

// Event code
// Adapted from lovedos

enum pt_event_type {
    EVENT_NULL,
    EVENT_QUIT,
    EVENT_KEY_DOWN,
    EVENT_KEY_UP,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_DOWN,
    EVENT_MOUSE_UP
};

typedef union {
    enum pt_event_type type;

    struct {
        enum pt_event_type type;
        int status;
    } quit;

    struct {
        enum pt_event_type type;
        int x, y;
        int dx, dy;
        int button;
    } mouse;

    struct {
        enum pt_event_type type;
        const char* key;
        bool isrepeat;
    } keyboard;

} pt_event;

void event_init();
pt_event* event_push(enum pt_event_type type);
pt_event* event_pop();
void event_shutdown();

#endif
