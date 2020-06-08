#ifndef __EVENTS_H__
#define __EVENTS_H__

#include <xcb/xcb.h>

void on_event(xcb_generic_event_t *event);

#endif
