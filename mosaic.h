#ifndef __MOSAIC_H__
#define __MOSAIC_H__

#include <xcb/xcb.h>

#include "client.h"
#include "monitor.h"

#ifndef VERSION
#define VERSION "0.0.0"
#endif

#define WMNAME "mosaic"

#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) < (B) ? (A) : (B))

typedef enum _Direction {
    D_UP,
    D_DOWN,
    D_LEFT,
    D_RIGHT
} Direction;

/* globals */
void quit();
void dump();
void toggle_bar();
void refresh_wmstatus();

/* monitors */
void scan_monitors();

/* windows */
void manage(xcb_window_t window);
Client * lookup(xcb_window_t window);
void forget(xcb_window_t window);
void find_focus(int fallback);
void focus(Client *client);
void unfocus(Client *client);

/* focus */
void focus_next_client();
void focus_previous_client();
void focus_next_monitor();
void focus_previous_monitor();
void focus_clicked_monitor(int x, int y);

/* focused monitor */ 
void focused_monitor_update_main_views(int by);
void focused_monitor_set_layout(Layout layout);
void focused_monitor_rotate_clockwise();
void focused_monitor_rotate_counter_clockwise();
void focused_monitor_set_tag(int tag);
void focused_monitor_toggle_tag(int tag);

/* focused client */
void focused_client_kill();
void focused_client_toggle_mode();
void focused_client_move(Direction direction);
void focused_client_to_next_monitor();
void focused_client_to_previous_monitor();
void focused_client_resize(int width, int height);
void focused_client_set_tag(int tag);
void focused_client_toggle_tag(int tag);

#endif
