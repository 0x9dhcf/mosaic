/*
 * Copyright (c) 2019-2020 Pierre Evenou
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

#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "mosaic.h"
#include "log.h"
#include "monitor.h"
#include "settings.h"

static int xcb_reply_contains_atom(xcb_get_property_reply_t *reply, xcb_atom_t atom);

int xcb_reply_contains_atom(xcb_get_property_reply_t *reply, xcb_atom_t atom)
{
    if (reply == NULL || xcb_get_property_value_length(reply) == 0)
        return 0;

    xcb_atom_t *atoms;
    if ((atoms = xcb_get_property_value(reply)) == NULL)
        return 0;

    for (int i = 0; i < xcb_get_property_value_length(reply) / (reply->format / 8); ++i)
        if (atoms[i] == atom)
            return 1;

    return 0;
}

void client_initialize(Client *c, xcb_window_t w)
{
    c->window = w;
    c->tiling_geometry = (Rectangle) {0};
    c->floating_geometry = (Rectangle) {0};
    c->border_width = g_border_width;
    c->border_color = g_normal_color;
    c->mode = MODE_TILED;
    c->state = STATE_ACCEPT_FOCUS;
    c->strut = (Strut){0};
    c->size_hints = (SizeHints){0};
    c->transient = XCB_NONE;
    c->monitor = NULL;
    c->tagset = -1;
    c->next = NULL;
    c->prev = NULL;

    xcb_change_save_set(g_xcb, XCB_SET_MODE_INSERT, w);

    /* manage the geometry of the window */
    /* TODO: xcb calls could be //lized */
    xcb_get_geometry_reply_t *geometry =
            xcb_get_geometry_reply(
                    g_xcb,
                    xcb_get_geometry(g_xcb, w),
                    NULL);

    if (geometry) {
        c->tiling_geometry = (Rectangle) {
                geometry->x,
                geometry->y,
                geometry->width,
                geometry->height};
        c->floating_geometry = (Rectangle) {
                geometry->x,
                geometry->y,
                geometry->width,
                geometry->height};
        free(geometry);
    }

    xcb_get_property_reply_t *transient = xcb_get_property_reply(
            g_xcb,
            xcb_get_property(
                    g_xcb,
                    0,
                    w,
                    XCB_ATOM_WM_TRANSIENT_FOR,
                    XCB_GET_PROPERTY_TYPE_ANY,
                    0,
                    UINT32_MAX),
            NULL);

    if (transient && xcb_get_property_value_length(transient) != 0) {
        xcb_icccm_get_wm_transient_for_from_reply(
                &c->transient,
                transient);
        client_set_mode(c, MODE_FLOATING);
    }

    free(transient);

    /* keep track of event of interrest */
    xcb_change_window_attributes(
            g_xcb,
            w,
            XCB_CW_EVENT_MASK,
            (const unsigned int []) {
                XCB_EVENT_MASK_ENTER_WINDOW |
                XCB_EVENT_MASK_LEAVE_WINDOW |
                XCB_EVENT_MASK_FOCUS_CHANGE |
                XCB_EVENT_MASK_PROPERTY_CHANGE |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY });

    /* grab buttons */
    xcb_grab_button(
            g_xcb,
            1,
            w,
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
            XCB_GRAB_MODE_SYNC,
            XCB_GRAB_MODE_SYNC,
            XCB_NONE,
            XCB_NONE,
            XCB_BUTTON_INDEX_ANY,
            XCB_MOD_MASK_ANY);

    /* apply the rules */
    xcb_get_property_reply_t *cr = xcb_get_property_reply(
            g_xcb,
            xcb_get_property(
                    g_xcb,
                    0,
                    w,
                    XCB_ATOM_WM_CLASS,
                    XCB_ATOM_STRING,
                    0, -1),
            NULL);
    if (cr) {
        char *p = xcb_get_property_value(cr);
        char *instance = p;
        char *class = p + strlen(instance) + 1;
        strncpy(c->instance, instance ? instance : "broken", 255);
        strncpy(c->class, class ? class : "broken", 255);
        int i = 0;
        while (g_rules[i].class_name) {
            if ((g_rules[i].instance_name && strcmp(g_rules[i].instance_name, instance) == 0) ||
                (g_rules[i].class_name && strcmp(g_rules[i].class_name, class) == 0)) {
                c->tagset = g_rules[i].tags;
                c->mode = g_rules[i].mode;
            }
            i++;
        }

        free(cr);
    }

    /* what the client tells us about itself */
    client_update_strut(c);
    client_update_size_hints(c);
    client_update_wm_hints(c);
    client_update_window_type(c);

    /* apply hint size */
    client_apply_size_hints(c);
}

void client_set_floating(Client *c, Rectangle *r)
{
    c->floating_geometry = *r;
    c->floating_geometry.width -= 2 * c->border_width;
    c->floating_geometry.height -= 2 * c->border_width;
    client_apply_size_hints(c);
}

void client_set_tiling(Client *c, Rectangle *r)
{
    c->tiling_geometry = *r;
    c->tiling_geometry.width -= 2 * c->border_width;
    c->tiling_geometry.height -= 2 * c->border_width;
}

void client_set_mode(Client *c, Mode m)
{
    c->saved_mode = c->mode;
    c->mode = m;
}

void client_set_tagset(Client *c, int tagset)
{
    c->saved_tagset = c->tagset;
    c->tagset = tagset;
}

void client_set_sticky(Client *c, int sticky)
{
    if (sticky) {
        c->state |= STATE_STICKY;
        client_set_mode(c, MODE_FLOATING);
        c->tiling_geometry = c->floating_geometry;
    } else {
        c->state &= ~ STATE_STICKY;
        c->mode = c->saved_mode;
        c->floating_geometry = c->tiling_geometry;
    }
}

void client_set_fullscreen(Client *c, int fullscreen)
{
    if (fullscreen && c->mode != MODE_FULLSCREEN) {
        c->border_width = 0;
        client_set_tagset(c, 0);
        c->tiling_geometry = c->floating_geometry;
        c->floating_geometry = c->monitor->geometry;
        client_set_mode(c, MODE_FULLSCREEN);
    } else {
        c->tagset = c->saved_tagset;
        c->border_width = g_border_width;
        c->mode = c->saved_mode;
        c->floating_geometry = c->tiling_geometry;
    }
}

void client_set_urgent(Client *c, int urgency)
{
    if (urgency)
        c->state |= STATE_URGENT;
    else
        c->state &= ~STATE_URGENT;

    xcb_change_window_attributes(
            g_xcb,
            c->window,
            XCB_CW_BORDER_PIXEL,
            (const unsigned int [])
            { urgency ?  g_urgent_color : g_normal_color });
}

void client_receive_focus(Client *c)
{
    xcb_change_window_attributes(
            g_xcb,
            c->window,
            XCB_CW_BORDER_PIXEL,
            (unsigned int []) { g_focused_color });

    if (c->monitor->layout == LT_NONE || c->mode != MODE_TILED)
        xcb_configure_window (
                g_xcb,
                c->window,
                XCB_CONFIG_WINDOW_STACK_MODE,
                (const unsigned int[]) { XCB_STACK_MODE_ABOVE });

    xcb_ewmh_set_active_window(&g_ewmh, g_screen_id, c->window);

    xcb_client_message_event_t e;
    e.type = XCB_CLIENT_MESSAGE;
    e.window = c->window;
    e.type = g_ewmh.WM_PROTOCOLS;
    e.format = 32;
    e.data.data32[0] = g_atoms[WM_TAKE_FOCUS];
    e.data.data32[1] = XCB_CURRENT_TIME;
    xcb_send_event(
            g_xcb,
            0,
            c->window,
            XCB_EVENT_MASK_NO_EVENT,
            (char*)&e);
}

void client_loose_focus(Client *c)
{
    xcb_change_window_attributes(
            g_xcb,
            c->window,
            XCB_CW_BORDER_PIXEL,
            (unsigned int []) { g_normal_color });
}

void client_hide(Client *c)
{
    Rectangle g = c->mode == MODE_TILED ?
        c->tiling_geometry : c->floating_geometry;

    xcb_grab_pointer(
                g_xcb,
                0,
                c->window,
                XCB_NONE,
                XCB_GRAB_MODE_SYNC,
                XCB_GRAB_MODE_SYNC,
                XCB_NONE,
                XCB_NONE,
                XCB_CURRENT_TIME);

    xcb_configure_window(
            g_xcb,
            c->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const int []) { -(g.width), -(g.height) });

    xcb_ungrab_pointer(g_xcb, XCB_CURRENT_TIME);
}

void client_show(Client *c)
{
    DEBUG_FUNCTION;

    Rectangle g = c->mode == MODE_TILED ?
        c->tiling_geometry : c->floating_geometry;

    DEBUG("client: %s, (%d, %d) [%d, %d]", c->instance, g.x, g.y, g.width, g.height);
    /*
     * "freeze" the window while moving it
     * e.g don't get enter_event that triggers focus_in
     * while moving a client
     */
    xcb_grab_pointer(
            g_xcb,
            0,
            c->window,
            XCB_NONE,
            XCB_GRAB_MODE_SYNC,
            XCB_GRAB_MODE_SYNC,
            XCB_NONE,
            XCB_NONE,
            XCB_CURRENT_TIME);

    xcb_configure_window(
            g_xcb,
            c->window,
            XCB_CONFIG_WINDOW_X |
            XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT |
            XCB_CONFIG_WINDOW_BORDER_WIDTH,
            (const int []) {
                g.x, g.y,
                g.width, g.height,
                c->border_width });

    if (c->mode == MODE_TILED)
        xcb_configure_window(
                g_xcb,
                c->window,
                XCB_CONFIG_WINDOW_STACK_MODE,
                (const int []) { XCB_STACK_MODE_BELOW });

    if (c->mode == MODE_FULLSCREEN)
        xcb_configure_window(
                g_xcb,
                c->window,
                XCB_CONFIG_WINDOW_STACK_MODE,
                (const int []) { XCB_STACK_MODE_ABOVE });

    xcb_ungrab_pointer(g_xcb, XCB_CURRENT_TIME);
}

int client_is_visible(Client *c)
{
    return (! c->tagset) || (c->tagset & c->monitor->tagset);
}

void client_notify(Client *c)
{
    xcb_configure_notify_event_t event;
    event.event = c->window;
    event.window = c->window;
    event.response_type = XCB_CONFIGURE_NOTIFY;
    if (c->mode == MODE_TILED) {
        event.x = c->tiling_geometry.x;
        event.y = c->tiling_geometry.y;
        event.width = c->tiling_geometry.width;
        event.height = c->tiling_geometry.height;
    } else {
        event.x = c->floating_geometry.x;
        event.y = c->floating_geometry.y;
        event.width = c->floating_geometry.width;
        event.height = c->floating_geometry.height;
    }
    event.border_width = c->border_width;
    event.above_sibling = XCB_NONE;
    event.override_redirect = 0;
    xcb_send_event(
            g_xcb,
            0,
            c->window,
            XCB_EVENT_MASK_STRUCTURE_NOTIFY,
            (char*)&event);
}

void client_apply_size_hints(Client *c)
{
    if (c->mode == MODE_FULLSCREEN)
        return;

    /* handle the size aspect ratio */
    double dx = c->floating_geometry.width - c->size_hints.base_width;
    double dy = c->floating_geometry.height - c->size_hints.base_height;
    double ratio = dx / dy;
    if (c->size_hints.max_aspect_ratio > 0 &&
            c->size_hints.min_aspect_ratio > 0 && ratio > 0) {
        if (ratio < c->size_hints.min_aspect_ratio) {
            dy = dx / c->size_hints.min_aspect_ratio + 0.5;
            c->floating_geometry.width  = dx + c->size_hints.base_width;
            c->floating_geometry.height = dy + c->size_hints.base_height;
        } else if (ratio > c->size_hints.max_aspect_ratio) {
            dx = dy * c->size_hints.max_aspect_ratio + 0.5;
            c->floating_geometry.width  = dx + c->size_hints.base_width;
            c->floating_geometry.height = dy + c->size_hints.base_height;
        }
    }

    /* handle the minimum size */
    c->floating_geometry.width = MAX(c->floating_geometry.width, c->size_hints.min_width);
    c->floating_geometry.height = MAX(c->floating_geometry.height, c->size_hints.min_height);

    /* handle the maximum size */
    if (c->size_hints.max_width > 0)
        c->floating_geometry.width = MIN(c->floating_geometry.width, c->size_hints.max_width);
    if (c->size_hints.max_height > 0)
        c->floating_geometry.height = MIN( c->floating_geometry.height, c->size_hints.max_height);

    /* handle the size increment */
    if (c->size_hints.width_increment > 0 && c->size_hints.height_increment > 0) {
        int t1 = c->floating_geometry.width;
        int t2 = c->floating_geometry.height;
        if (c->size_hints.base_width > t1)
            t1 = 0;
        else
            t1 -= c->size_hints.base_width;

        if (c->size_hints.base_height > t2)
            t2 = 0;
        else
            t2 -= c->size_hints.base_height;

        c->floating_geometry.width -= t1 % c->size_hints.width_increment;
        c->floating_geometry.height -= t2 % c->size_hints.height_increment;
    }
}

int client_update_strut(Client *c)
{
    c->strut = (Strut){0};

    xcb_ewmh_wm_strut_partial_t strut;
    if (xcb_ewmh_get_wm_strut_partial_reply(
            &g_ewmh,
            xcb_ewmh_get_wm_strut_partial(&g_ewmh, c->window),
            &strut,
            NULL) == 1) {
        c->strut.top = strut.top;
        c->strut.bottom = strut.bottom;
        c->strut.left = strut.left;
        c->strut.right = strut.right;
        return 1;
    }
    return 0;
}

/* TODO update user size (XCB_ICCCM_SIZE_HINT_US_POSITION etc.) */
int client_update_size_hints(Client *c)
{
    int refresh = 0;
    xcb_get_property_reply_t *normal_hints = xcb_get_property_reply(
            g_xcb,
            xcb_icccm_get_wm_normal_hints(g_xcb, c->window),
            NULL);
    if (! normal_hints) {
        INFO("Can't get normal hints.");
        return 0;
    }

    c->size_hints.base_width = c->size_hints.base_height = 0;
    c->size_hints.width_increment = c->size_hints.height_increment = 0;
    c->size_hints.max_width = c->size_hints.max_height = 0;
    c->size_hints.min_width = c->size_hints.min_height = 0;
    c->size_hints.max_aspect_ratio = c->size_hints.min_aspect_ratio = 0.0;

    xcb_size_hints_t size;
    if (xcb_icccm_get_wm_size_hints_from_reply(&size, normal_hints)) {

        /* base size */
        if (size.flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE) {
            c->size_hints.base_width = size.base_width;
            c->size_hints.base_height = size.base_height;
        } else {
            /* note: not using min size as fallback */
            c->size_hints.base_width = c->size_hints.base_height = 0;
        }

        /* max size */
        if (size.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) {
            c->size_hints.max_width = MAX(size.max_width, 1);
            c->size_hints.max_height = MAX(size.max_height, 1);
        } else {
            c->size_hints.max_width = c->size_hints.max_height = UINT32_MAX;
        }

        /* min size */
        if (size.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
            c->size_hints.min_width = size.min_width;
            c->size_hints.min_height = size.min_height;
        } else {
            /* according to ICCCM 4.1.23 base size
             * should be used as a fallback */
            c->size_hints.min_width = c->size_hints.base_width;
            c->size_hints.min_height = c->size_hints.base_height;
        }

        /* increments */
        if (size.flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC) {
            c->size_hints.width_increment = size.width_inc;
            c->size_hints.height_increment = size.height_inc;
        } else {
            c->size_hints.width_increment = c->size_hints.height_increment = 1;
        }

        /* aspect */
        int max_aspect_num;
        int max_aspect_den;
        int min_aspect_num;
        int min_aspect_den;
        if (size.flags & XCB_ICCCM_SIZE_HINT_P_ASPECT) {
            max_aspect_num = size.max_aspect_num;
            max_aspect_den = MAX(size.max_aspect_den, 1);
            min_aspect_num = size.min_aspect_num;
            min_aspect_den = MAX(size.min_aspect_den, 1);
        } else {
            max_aspect_num = UINT32_MAX;
            max_aspect_den = 1;
            min_aspect_num = 1;
            min_aspect_den = UINT32_MAX;
        }
        c->size_hints.max_aspect_ratio = (double)max_aspect_num / (double)max_aspect_den;
        c->size_hints.min_aspect_ratio = (double)min_aspect_num / (double)min_aspect_den;

        /* XXX: the client should be "fixed" but definitely not "sticky"
        if (client->max_width && client->max_height &&
                client->max_width == client->min_width &&
                client->max_height == client->min_height)
            client_set_sticky(client, 1);
        */
        refresh = 1;
    }

    free(normal_hints);
    return refresh;
}

int client_update_wm_hints(Client *c)
{
    int refresh = 0;

    xcb_get_property_reply_t *hints = xcb_get_property_reply(
            g_xcb,
            xcb_icccm_get_wm_hints(g_xcb, c->window),
            NULL);

    if (! hints) {
        INFO("Can't get wm hints.");
        return 0;
    }

    c->state |= STATE_ACCEPT_FOCUS;
    client_set_urgent(c, 0);

    if (xcb_get_property_value_length(hints) != 0) {
        xcb_icccm_wm_hints_t wm_hints;
        if (xcb_icccm_get_wm_hints_from_reply(&wm_hints, hints)) {
            if (wm_hints.flags & XCB_ICCCM_WM_HINT_INPUT) {
                if (wm_hints.input)
                    c->state |= STATE_ACCEPT_FOCUS;
                else
                    c->state &= ~STATE_ACCEPT_FOCUS;
            }
            client_set_urgent(c, xcb_icccm_wm_hints_get_urgency(&wm_hints));
            refresh = 1;
        }
    }

    free(hints);

    return refresh;
}

int client_update_window_type(Client *c)
{
    int refresh = 0;
    xcb_get_property_reply_t *type = xcb_get_property_reply(
            g_xcb,
            xcb_get_property(
                    g_xcb,
                    0,
                    c->window,
                    g_ewmh._NET_WM_WINDOW_TYPE,
                    XCB_GET_PROPERTY_TYPE_ANY,
                    0,
                    UINT32_MAX),
            NULL);
    if (! type) {
        INFO("Can't get window type.");
        return 0;
    }

    if (xcb_reply_contains_atom(type, g_ewmh._NET_WM_WINDOW_TYPE_DIALOG) ||
             xcb_reply_contains_atom(type, g_ewmh._NET_WM_WINDOW_TYPE_UTILITY) ||
             xcb_reply_contains_atom(type, g_ewmh._NET_WM_WINDOW_TYPE_TOOLBAR) ||
             xcb_reply_contains_atom(type, g_ewmh._NET_WM_WINDOW_TYPE_SPLASH)) {
        c->mode = MODE_FLOATING;
        refresh = 1;
    }

    if (xcb_reply_contains_atom(type, g_ewmh._NET_WM_WINDOW_TYPE_DOCK)) {
        c->state &= ~STATE_ACCEPT_FOCUS;
        c->border_width = 0;
        client_set_sticky(c, 1);
        client_set_tagset(c, 0);
    }

    free(type);
    return refresh;
}

#define CLIENT_MATCH_MODE_AND_STATE(c, m, s)\
        ((m == MODE_ANY || c->mode == m) && (c->state & s) ==  s && client_is_visible(c))

Client *client_next(Client *c, Mode mode, State state)
{
    /* find the first successor matching */
    for (Client *ic = c->next; ic; ic = ic->next)
        if (CLIENT_MATCH_MODE_AND_STATE(ic, mode, state))
            return ic;

    /* if not found then find the first one matching from the head */
    for (Client *ic = c->monitor->head; ic && ic != c; ic = ic->next)
        if (CLIENT_MATCH_MODE_AND_STATE(c, mode, state))
            return ic;

    return NULL;
}

Client *client_previous(Client *c, Mode mode, State state)
{
    /* find the first ancestor matching */
    for (Client *ic = c->prev; ic; ic = ic->prev)
        if (CLIENT_MATCH_MODE_AND_STATE(ic, mode, state))
            return ic;

    /* if not found then find the first one matching from the tail */
    for (Client *ic = c->monitor->tail; ic && ic != c; ic = ic->prev)
        if (CLIENT_MATCH_MODE_AND_STATE(ic, mode, state))
            return ic;

    return NULL;
}

#undef CLIENT_MATCH_MODE_AND_STATE
