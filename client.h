/*
 * Copyright (c) 2019 Pierre Evenou
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __MWM_CLIENT_H__
#define __MWM_CLIENT_H__

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_ewmh.h>

typedef struct _Monitor Monitor;

typedef enum _Mode {
    MODE_ANY,
    MODE_TILED,
    MODE_FLOATING
} Mode;

typedef int State;

#define STATE_ANY           0x00
#define STATE_FOCUSABLE     0x01
#define STATE_STICKY        0x02
#define STATE_FULLSCREEN    0x04
#define STATE_URGENT        0x08

typedef struct _Client {
    xcb_window_t    window;
    int             t_x;
    int             t_y;
    int             t_width;
    int             t_height;
    int             f_x;
    int             f_y;
    int             f_width;
    int             f_height;
    int             border_width;
    int             border_color;
    Mode            mode;
    State           state;
    int             reserved_top;
    int             reserved_bottom;
    int             reserved_left;
    int             reserved_right;
    int             base_width;
    int             base_height;
    int             width_increment;
    int             height_increment;
    int             min_width;
    int             min_height;
    int             max_width;
    int             max_height;
    double          min_aspect_ratio;
    double          max_aspect_ratio;
    xcb_window_t    transient;
    Monitor         *monitor;
    int             tagset;
    int             saved_tagset;
    struct _Client  *prev;
    struct _Client  *next;
} Client;

#define IS_CLIENT_STATE(c, s) ((c->state & s) == (s))
#define IS_CLIENT_STATE_NOT(c, s) (! IS_CLIENT_STATE(c, s))
#define CLIENT_SET_STATE(c, s) ((c->state |= s))
#define CLIENT_UNSET_STATE(c, s) ((c->state &= ~s))
#define IS_VISIBLE(c) ((c->tagset & c->monitor->tagset) || ! c->tagset)

void client_initialize(Client *client, xcb_window_t window);
void client_set_focusable(Client *client, int focusable);
void client_set_sticky(Client *client, int sticky);
void client_set_fullscreen(Client *client, int fullscreen);
void client_set_urgent(Client *client, int urgency);
void client_hide(Client *client);
void client_show(Client *client);
void client_apply_size_hints(Client *client);
void client_set_focused(Client *client, int focus);
int client_update_reserved(Client *client);
int client_update_size_hints(Client *client);
int client_update_wm_hints(Client *client);
int client_update_window_type(Client *client);
Client *client_next(Client *client, Mode mode, State state);
Client *client_previous(Client *client, Mode mode, State state);

#endif
