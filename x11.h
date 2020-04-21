/*
 * Copyright (c) 2019, 2020 Pierre Evenou
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

#ifndef __MWM_X11_H__
#define __MWM_X11_H__

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xkb.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-x11.h>


/* atoms */
enum {
    WM_TAKE_FOCUS,
    MWM_MONITOR_TAGS,
    MWM_MONITOR_TAGSET,
    MWM_FOCUSED,
    MWM_FOCUSED_TAGSET,
    MWM_ATOM_COUNT
};

extern xcb_connection_t       *g_xcb;
extern int                     g_screen_id;
extern xcb_screen_t           *g_screen;
extern xcb_window_t            g_root;
extern xcb_visualtype_t       *g_visual;
extern xcb_ewmh_connection_t   g_ewmh;
extern xcb_atom_t              g_atoms[MWM_ATOM_COUNT];
extern struct xkb_state       *g_xkb_state;

void x11_setup();
void x11_cleanup();

#endif

