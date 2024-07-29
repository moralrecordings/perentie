#ifndef PERENTIE_EVENT_H
#define PERENTIE_EVENT_H

// Event code
// Adapted from lovedos

enum pt_event_type {
    EVENT_NULL,
    EVENT_QUIT,
    EVENT_KEYBOARD_PRESSED,
    EVENT_KEYBOARD_RELEASED,
    EVENT_MOUSE_MOVED,
    EVENT_MOUSE_PRESSED,
    EVENT_MOUSE_RELEASED
};

typedef union {
    enum pt_event_type type;

    struct {
        int type;
        int status;
    } quit;

    struct {
        int type;
        int x, y;
        int dx, dy;
        int button;
    } mouse;

    struct {
        int type;
        const char* key;
        int isrepeat;
    } keyboard;

} pt_event;

void event_init();
pt_event* create_event(enum pt_event_type type);
void event_push(pt_event* event);
pt_event* event_pop();
void destroy_event(pt_event* event);
void event_shutdown();

#endif
