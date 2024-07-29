#include <stdlib.h>
#include <string.h>

#include "event.h"

#define EVENT_QUEUE_SIZE 2048
#define EVENT_QUEUE_MASK (EVENT_QUEUE_SIZE - 1)
static pt_event* event_queue[EVENT_QUEUE_SIZE] = { 0 };
static size_t event_readi = 0;
static size_t event_writei = 0;

void event_init()
{
    memset(event_queue, 0, sizeof(pt_event*) * EVENT_QUEUE_SIZE);
}

pt_event* create_event(enum pt_event_type type)
{
    pt_event* ev = (pt_event*)calloc(1, sizeof(pt_event));
    return ev;
}

void event_push(pt_event* event)
{
    if (!event)
        return;
    // If the queue is full, drop the event
    if (event_readi == ((event_writei + 1) % EVENT_QUEUE_SIZE)) {
        free(event);
        return;
    }
    event_queue[event_writei] = event;
    event_writei = (event_writei + 1) & EVENT_QUEUE_MASK;
}

pt_event* event_pop()
{
    if (event_writei == event_readi)
        return NULL;
    pt_event* result = event_queue[event_readi];
    event_readi = (event_readi + 1) & EVENT_QUEUE_MASK;
    return result;
}

void destroy_event(pt_event* event)
{
    if (!event)
        return;
    free(event);
}

void event_shutdown()
{
    pt_event* ev;
    while ((ev = event_pop())) {
        destroy_event(ev);
    }
}
