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

#include "log.h"
#include "x11.h"

xcb_connection_t       *g_xcb;
int                     g_screen_id;
xcb_screen_t           *g_screen;
xcb_window_t            g_root;
xcb_visualtype_t       *g_visual;
xcb_colormap_t          g_colormap;
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

void
x11_setup()
{
    g_xcb = xcb_connect(0, &g_screen_id);
    if (xcb_connection_has_error(g_xcb))
       FATAL("can't get xcb connection.");

    xcb_prefetch_extension_data(g_xcb, &xcb_xkb_id);
    xcb_prefetch_extension_data(g_xcb, &xcb_randr_id);

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

    xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(g_xcb));
    for (int i = 0; i < g_screen_id; ++i)
        xcb_screen_next(&it);
    g_screen = it.data;
    g_root = g_screen->root;

    g_visual = NULL;
    xcb_depth_iterator_t depth_iter = xcb_screen_allowed_depths_iterator(g_screen);
    for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
        xcb_visualtype_iterator_t visual_iter;

        visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
        for (; visual_iter.rem; xcb_visualtype_next(&visual_iter)) {
            if (g_screen->root_visual == visual_iter.data->visual_id) {
                g_visual = visual_iter.data;
                break;
            }
        }
    }
    if (! g_visual)
        FATAL("can't get visual.");

    g_colormap = xcb_generate_id(g_xcb);
    xcb_create_colormap(
            g_xcb,
            XCB_COLORMAP_ALLOC_NONE,
            g_colormap,
            g_root,
            g_visual->visual_id);

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

void
x11_cleanup()
{
    xcb_ewmh_connection_wipe(&g_ewmh);
    xcb_disconnect(g_xcb);
}
