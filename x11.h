#ifndef __X11_H__
#define __X11_H__

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

extern xcb_connection_t         *g_xcb;
extern int                      g_screen_id;
extern xcb_screen_t             *g_screen;
extern xcb_window_t             g_root;
extern xcb_visualtype_t         *g_visual;
extern xcb_colormap_t           g_colormap;
extern xcb_ewmh_connection_t    g_ewmh;
extern xcb_atom_t               g_atoms[MWM_ATOM_COUNT];
extern struct xkb_state         *g_xkb_state;

void x11_setup();
void x11_cleanup();

#endif
