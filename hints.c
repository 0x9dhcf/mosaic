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

#include <string.h>
#include <xcb/xcb_atom.h>

#include "hints.h"
#include "log.h"
#include "mwm.h"

void hints_set_monitor(Monitor *monitor)
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

void hints_set_focused(Client *client)
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
