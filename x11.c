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

#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xkb.h>
#include <xcb/randr.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-x11.h>

#include "x11.h"
#include "log.h"

xcb_connection_t       *g_xcb;
int                     g_screen_id;
xcb_screen_t           *g_screen;
xcb_window_t            g_root;
xcb_visualtype_t       *g_visual;
xcb_ewmh_connection_t   g_ewmh;
xcb_atom_t              g_atoms[MWM_ATOM_COUNT];
struct xkb_state       *g_xkb_state;

/* static variables */
static const char *atom_names[MWM_ATOM_COUNT] = {
    "WM_TAKE_FOCUS",
    "MWM_MONITOR_TAGS",
    "MWM_MONITOR_TAGSET",
    "MWM_FOCUSED",
    "MWM_FOCUSED_TAGSET",
};

void x11_setup()
{
    g_xcb = xcb_connect(0, &g_screen_id);
    if (xcb_connection_has_error(g_xcb))
        FATAL("can't get xcb connection.");

    xcb_prefetch_extension_data(g_xcb, &xcb_xkb_id);
    xcb_prefetch_extension_data(g_xcb, &xcb_randr_id);

    xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(g_xcb));
    for (int i = 0; i < g_screen_id; ++i)
        xcb_screen_next(&it);
    g_screen = it.data;
    g_root = g_screen->root;
    const xcb_query_extension_reply_t *ext_reply;
    ext_reply = xcb_get_extension_data(g_xcb, &xcb_xkb_id);
    if (!ext_reply->present)
        FATAL("no xkb extension on this server.");
    ext_reply = xcb_get_extension_data(g_xcb, &xcb_randr_id);
    if (!ext_reply->present)
        FATAL("no randr extension on this server.");

    xcb_intern_atom_cookie_t *init_atoms_cookie = xcb_ewmh_init_atoms(
            g_xcb,
            &g_ewmh);

    if (! xcb_ewmh_init_atoms_replies(&g_ewmh, init_atoms_cookie, NULL))
        free(init_atoms_cookie);

    xcb_intern_atom_cookie_t cookies[MWM_ATOM_COUNT];
    for (int i = 0; i < MWM_ATOM_COUNT; ++i)
        cookies[i] = xcb_intern_atom(
                g_xcb,
                0,
                strlen(atom_names[i]),
                atom_names[i]);

    for (int i = 0; i < MWM_ATOM_COUNT; ++i) {
        xcb_intern_atom_reply_t *a = xcb_intern_atom_reply(
                g_xcb,
                cookies[i],
                NULL);

        if (a) {
            g_atoms[i] = a->atom;
            free(a);
        }
    }
}

void x11_cleanup()
{
    xcb_ewmh_connection_wipe(&g_ewmh);
    xcb_disconnect(g_xcb);
}
