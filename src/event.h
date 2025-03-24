#ifndef PERENTIE_EVENT_H
#define PERENTIE_EVENT_H

#include <stdbool.h>
#include <stdint.h>
// Event code
// Adapted from lovedos

enum pt_event_type {
    EVENT_NULL,
    EVENT_START,
    EVENT_QUIT,
    EVENT_RESET,
    EVENT_KEY_DOWN,
    EVENT_KEY_UP,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_DOWN,
    EVENT_MOUSE_UP
};

enum pt_key_flag {
    KEY_FLAG_CTRL = 1,
    KEY_FLAG_ALT = 2,
    KEY_FLAG_SHIFT = 4,
    KEY_FLAG_NUM = 8,
    KEY_FLAG_CAPS = 16,
    KEY_FLAG_SCRL = 32,
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
        uint8_t flags;
        bool isrepeat;
    } keyboard;

    struct {
        enum pt_event_type type;
        char* state_path;
    } reset;

} pt_event;

void event_init();
pt_event* event_push(enum pt_event_type type);
pt_event* event_pop();
void event_shutdown();

#endif
