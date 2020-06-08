#include <string.h>

#include "hints.h"
#include "log.h"
#include "mosaic.h"
#include "x11.h"

void
hints_set_monitor(Monitor *monitor)
{
    xcb_change_property(
            g_xcb,
            XCB_PROP_MODE_REPLACE,
            g_root,
            g_atoms[MWM_MONITOR_TAGS],
            XCB_ATOM_CARDINAL, 32, 32, monitor->tags);

    xcb_change_property(
            g_xcb,
            XCB_PROP_MODE_REPLACE,
            g_root,
            g_atoms[MWM_MONITOR_TAGSET],
            XCB_ATOM_INTEGER, 32, 1, &monitor->tagset);
}

void
hints_set_focused(Client *client)
{
    int notag = 0;
    xcb_change_property(
            g_xcb,
            XCB_PROP_MODE_REPLACE,
            g_root,
            g_atoms[MWM_FOCUSED],
            XCB_ATOM_WINDOW, 32, 1,
            client ? &client->window : &g_root);

    xcb_change_property(
            g_xcb,
            XCB_PROP_MODE_REPLACE,
            g_root,
            g_atoms[MWM_FOCUSED_TAGSET],
            XCB_ATOM_INTEGER, 32, 1,
            client ? &client->tagset : &notag);
}
