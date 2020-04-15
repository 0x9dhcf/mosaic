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

#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "mwm.h"
#include "log.h"
#include "monitor.h"
#include "settings.h"

static int xcb_reply_contains_atom(xcb_get_property_reply_t *reply, xcb_atom_t atom);
static void center_client(Client *c, Client *on);

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

void center_client(Client *c, Client *on)
{
    Rectangle r = on->mode == MODE_TILED ? on->tiling_geometry : on->floating_geometry;
    c->floating_geometry.x = (r.x + r.width / 2) - c->floating_geometry.width / 2;
    c->floating_geometry.y = (r.y + r.height / 2) - c->floating_geometry.height / 2;
}

void client_initialize(Client *client, xcb_window_t window)
{
    client->window = window;
    client->tiling_geometry = (Rectangle) {0};
    client->floating_geometry = (Rectangle) {0};
    client->border_width = g_border_width;
    client->border_color = g_normal_color;
    client->mode = MODE_TILED;
    client->state = STATE_ACCEPT_FOCUS;
    client->strut = (Strut){0};
    client->size_hints = (SizeHints){0};
    client->transient = 0;
    client->transient = XCB_NONE;
    client->monitor = NULL;
    client->tagset = -1;
    client->next = NULL;
    client->prev = NULL;

    xcb_change_save_set(g_xcb, XCB_SET_MODE_INSERT, window);

    /* manage the geometry of the window */
    /* TODO: xcb calls could be //lized */
    xcb_get_geometry_reply_t *geometry =
            xcb_get_geometry_reply(
                    g_xcb,
                    xcb_get_geometry(g_xcb, window),
                    NULL);

    if (geometry) {
        client->tiling_geometry = (Rectangle) {
                geometry->x,
                geometry->y,
                geometry->width,
                geometry->height};
        client->floating_geometry = (Rectangle) {
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
                    client->window,
                    XCB_ATOM_WM_TRANSIENT_FOR,
                    XCB_GET_PROPERTY_TYPE_ANY,
                    0,
                    UINT32_MAX),
            NULL);

    if (transient && xcb_get_property_value_length(transient) != 0)
        xcb_icccm_get_wm_transient_for_from_reply(
                &client->transient,
                transient);
        free(transient);

    /* keep track of event of interrest */
    xcb_change_window_attributes(
            g_xcb,
            window,
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
            window,
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
                    client->window,
                    XCB_ATOM_WM_CLASS,
                    XCB_ATOM_STRING,
                    0, -1),
            NULL);
    if (cr) {
        char *p = xcb_get_property_value(cr);
        char *instance = p;
        char *class = p + strlen(instance) + 1;
        int i = 0;
        while (g_rules[i].class_name) {
            if ((g_rules[i].instance_name && strcmp(g_rules[i].instance_name, instance) == 0) ||
                (g_rules[i].class_name && strcmp(g_rules[i].class_name, class) == 0)) {
                client->tagset = g_rules[i].tags;
                client->mode = g_rules[i].mode;
            }
            i++;
        }

        free(cr);
    }

    /* what the client tells us about itself */
    client_update_reserved(client);
    client_update_size_hints(client);
    client_update_wm_hints(client);
    client_update_window_type(client);

    /* apply hint size */
    client_apply_size_hints(client);
}

void client_set_sticky(Client *client, int sticky)
{
    if (sticky) {
        CLIENT_SET_STATE(client, STATE_STICKY);
        client->saved_mode = client->mode;
        client->mode = MODE_FLOATING;
        /* we use the tiled geometry as absolute geometry
         * (relative to monitors) */
        client->tiling_geometry = client->floating_geometry;
    } else {
        CLIENT_UNSET_STATE(client, STATE_STICKY);
        client->mode = client->saved_mode;
    }
}

void client_set_fullscreen(Client *client, int fullscreen)
{
    if (fullscreen && client->mode != MODE_FULLSCREEN) {
        client->border_width = 0;
        client->saved_tagset = client->tagset;
        client->tagset = 0; /* visible on all monitor's tags */
        client->saved_mode = client->mode;
        client->mode = MODE_FULLSCREEN;
    } else {
        client->tagset = client->saved_tagset;
        client->border_width = g_border_width;
        client->mode = client->saved_mode;;
    }
}

void client_set_urgent(Client *client, int urgency)
{
    if (urgency)
        CLIENT_SET_STATE(client, STATE_URGENT);
    else
        CLIENT_UNSET_STATE(client, STATE_URGENT);

    xcb_change_window_attributes(
            g_xcb,
            client->window,
            XCB_CW_BORDER_PIXEL,
            (unsigned int []) { urgency ?  g_urgent_color : g_normal_color });
}

void client_hide(Client *client)
{
    xcb_grab_pointer(
                g_xcb,
                0,
                client->window,
                XCB_NONE,
                XCB_GRAB_MODE_SYNC,
                XCB_GRAB_MODE_SYNC,
                XCB_NONE,
                XCB_NONE,
                XCB_CURRENT_TIME);

    xcb_configure_window(
            g_xcb,
            client->window,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const int []) {
                    -(client->monitor->geometry.width),
                    -(client->monitor->geometry.height) });

    xcb_ungrab_pointer(g_xcb, XCB_CURRENT_TIME);
}

void client_show(Client *client)
{
    if (IS_VISIBLE(client)) {
        Rectangle geometry;
        if (client->mode == MODE_TILED) {
            geometry = client->tiling_geometry;
        } else if (client->mode == MODE_FLOATING) { 
            if (client->transient) {
                Client *t = lookup(client->transient);
                if (t) /* transient for might be closed yet */
                    center_client(t, client);
            }
            if (IS_CLIENT_STATE(client, STATE_STICKY)) {
                client->floating_geometry.x =
                    client->monitor->geometry.x +
                    client->tiling_geometry.x;
                client->floating_geometry.y =
                    client->monitor->geometry.y +
                    client->tiling_geometry.y;
            }
            geometry = client->floating_geometry;
        } else { /* FULLSCREEN */
            geometry = client->monitor->geometry;
        }

        /*
         * "freeze" the window while moving it
         * e.g don't get enter_event that triggers focus_in
         * while moving a client
         */
        xcb_grab_pointer(
                g_xcb,
                0,
                client->window,
                XCB_NONE,
                XCB_GRAB_MODE_SYNC,
                XCB_GRAB_MODE_SYNC,
                XCB_NONE,
                XCB_NONE,
                XCB_CURRENT_TIME);

        xcb_configure_window(
                g_xcb,
                client->window,
                XCB_CONFIG_WINDOW_X |
                XCB_CONFIG_WINDOW_Y |
                XCB_CONFIG_WINDOW_WIDTH |
                XCB_CONFIG_WINDOW_HEIGHT |
                XCB_CONFIG_WINDOW_BORDER_WIDTH,
                (const int []) {
                    geometry.x, geometry.y,
                    geometry.width, geometry.height,
                    client->border_width });

        if (client->mode == MODE_TILED)
            xcb_configure_window(
                    g_xcb,
                    client->window,
                    XCB_CONFIG_WINDOW_STACK_MODE,
                    (const int []) { XCB_STACK_MODE_BELOW });

        if (client->mode == MODE_FULLSCREEN)
            xcb_configure_window(
                    g_xcb,
                    client->window,
                    XCB_CONFIG_WINDOW_STACK_MODE,
                    (const int []) { XCB_STACK_MODE_ABOVE });

        xcb_ungrab_pointer(g_xcb, XCB_CURRENT_TIME);
    }
}

void client_apply_size_hints(Client *client)
{
    if (client->mode == MODE_FULLSCREEN)
        return;

    /* handle the size aspect ratio */
    double dx = client->floating_geometry.width - client->size_hints.base_width;
    double dy = client->floating_geometry.height - client->size_hints.base_height;
    double ratio = dx / dy;
    if (client->size_hints.max_aspect_ratio > 0 &&
            client->size_hints.min_aspect_ratio > 0 && ratio > 0) {
        if (ratio < client->size_hints.min_aspect_ratio) {
            dy = dx / client->size_hints.min_aspect_ratio + 0.5;
            client->floating_geometry.width  = dx + client->size_hints.base_width;
            client->floating_geometry.height = dy + client->size_hints.base_height;
        } else if (ratio > client->size_hints.max_aspect_ratio) {
            dx = dy * client->size_hints.max_aspect_ratio + 0.5;
            client->floating_geometry.width  = dx + client->size_hints.base_width;
            client->floating_geometry.height = dy + client->size_hints.base_height;
        }
    }

    /* handle the minimum size */
    client->floating_geometry.width = MAX(
            client->floating_geometry.width,
            client->size_hints.min_width);
    client->floating_geometry.height = MAX(
            client->floating_geometry.height,
            client->size_hints.min_height);

    /* handle the maximum size */
    if (client->size_hints.max_width > 0) {
        client->floating_geometry.width = MIN(
                client->floating_geometry.width,
                client->size_hints.max_width);
    }
    if (client->size_hints.max_height > 0) {
        client->floating_geometry.height = MIN(
                client->floating_geometry.height,
                client->size_hints.max_height);
    }

    /* handle the size increment */
    if (client->size_hints.width_increment > 0 &&
            client->size_hints.height_increment > 0) {
        int t1 = client->floating_geometry.width;
        int t2 = client->floating_geometry.height;
        if (client->size_hints.base_width > t1)
            t1 = 0;
        else
            t1 -= client->size_hints.base_width;

        if (client->size_hints.base_height > t2)
            t2 = 0;
        else
            t2 -= client->size_hints.base_height;

        client->floating_geometry.width -=
            t1 % client->size_hints.width_increment;
        client->floating_geometry.height -=
            t2 % client->size_hints.height_increment;
    }
}

int client_update_reserved(Client *client)
{
    client->strut = (Strut){0};

    xcb_ewmh_wm_strut_partial_t strut;
    if (xcb_ewmh_get_wm_strut_partial_reply(
            &g_ewmh,
            xcb_ewmh_get_wm_strut_partial(&g_ewmh, client->window),
            &strut,
            NULL) == 1) {
        client->strut.top = strut.top;
        client->strut.bottom = strut.bottom;
        client->strut.left = strut.left;
        client->strut.right = strut.right;
        return 1;
    }
    return 0;
}

/* TODO update user size (XCB_ICCCM_SIZE_HINT_US_POSITION etc.) */
int client_update_size_hints(Client *client)
{
    int refresh = 0;
    xcb_get_property_reply_t *normal_hints = xcb_get_property_reply(
            g_xcb,
            xcb_icccm_get_wm_normal_hints(g_xcb, client->window),
            NULL);
    if (! normal_hints) {
        INFO("Can't get normal hints.");
        return 0;
    }

    client->size_hints.base_width = client->size_hints.base_height = 0;
    client->size_hints.width_increment = client->size_hints.height_increment = 0;
    client->size_hints.max_width = client->size_hints.max_height = 0;
    client->size_hints.min_width = client->size_hints.min_height = 0;
    client->size_hints.max_aspect_ratio = client->size_hints.min_aspect_ratio = 0.0;

    xcb_size_hints_t size;
    if (xcb_icccm_get_wm_size_hints_from_reply(&size, normal_hints)) {

        /* base size */
        if (size.flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE) {
            client->size_hints.base_width = size.base_width;
            client->size_hints.base_height = size.base_height;
        } else {
            /* note: not using min size as fallback */
            client->size_hints.base_width = client->size_hints.base_height = 0;
        }

        /* max size */
        if (size.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) {
            client->size_hints.max_width = MAX(size.max_width, 1);
            client->size_hints.max_height = MAX(size.max_height, 1);
        } else {
            client->size_hints.max_width = client->size_hints.max_height = UINT32_MAX;
        }

        /* min size */
        if (size.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
            client->size_hints.min_width = size.min_width;
            client->size_hints.min_height = size.min_height;
        } else {
            /* according to ICCCM 4.1.23 base size
             * should be used as a fallback */
            client->size_hints.min_width = client->size_hints.base_width;
            client->size_hints.min_height = client->size_hints.base_height;
        }

        /* increments */
        if (size.flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC) {
            client->size_hints.width_increment = size.width_inc;
            client->size_hints.height_increment = size.height_inc;
        } else {
            client->size_hints.width_increment = client->size_hints.height_increment = 1;
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
        client->size_hints.max_aspect_ratio = (double)max_aspect_num / (double)max_aspect_den;
        client->size_hints.min_aspect_ratio = (double)min_aspect_num / (double)min_aspect_den;

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

int client_update_wm_hints(Client *client)
{
    int refresh = 0;

    xcb_get_property_reply_t *hints = xcb_get_property_reply(
            g_xcb,
            xcb_icccm_get_wm_hints(g_xcb, client->window),
            NULL);

    if (! hints) {
        INFO("Can't get wm hints.");
        return 0;
    }

    CLIENT_SET_STATE(client, STATE_ACCEPT_FOCUS);
    client_set_urgent(client, 0);

    if (xcb_get_property_value_length(hints) != 0) {
        xcb_icccm_wm_hints_t wm_hints;
        if (xcb_icccm_get_wm_hints_from_reply(&wm_hints, hints)) {
            if (wm_hints.flags & XCB_ICCCM_WM_HINT_INPUT) {
                if (wm_hints.input)
                    CLIENT_SET_STATE(client, STATE_ACCEPT_FOCUS);
                else
                    CLIENT_UNSET_STATE(client, STATE_ACCEPT_FOCUS);
            }
            client_set_urgent(client, xcb_icccm_wm_hints_get_urgency(&wm_hints));
            refresh = 1;
        }
    }

    free(hints);

    return refresh;
}

int client_update_window_type(Client *client)
{
    int refresh = 0;
    xcb_get_property_reply_t *type = xcb_get_property_reply(
            g_xcb,
            xcb_get_property(
                    g_xcb,
                    0,
                    client->window,
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
        client->mode = MODE_FLOATING;
        refresh = 1;
    }

    if (xcb_reply_contains_atom(type, g_ewmh._NET_WM_WINDOW_TYPE_DOCK)) {
        CLIENT_UNSET_STATE(client, STATE_ACCEPT_FOCUS);
        client_set_sticky(client, 1);
        client->border_width = 0;
        client->tagset = 0;
    }

    free(type);
    return refresh;
}

void client_set_focused(Client *client, int focus)
{
    xcb_change_window_attributes(
            g_xcb,
            client->window,
            XCB_CW_BORDER_PIXEL,
            (unsigned int []) { focus ?  g_focused_color : g_normal_color });
    if (focus && (client->monitor->layout == LT_NONE || client->mode != MODE_TILED))
        xcb_configure_window (
                g_xcb,
                client->window,
                XCB_CONFIG_WINDOW_STACK_MODE,
                (const unsigned int[]) { XCB_STACK_MODE_ABOVE });
}

#define CLIENT_MATCH_MODE_AND_STATE(c, m, s)\
        ((m == MODE_ANY || c->mode == m) && IS_CLIENT_STATE(c, s) && IS_VISIBLE(c))

Client *client_next(Client *client, Mode mode, State state)
{
    /* find the first successor matching */
    for (Client *c = client->next; c; c = c->next)
        if (CLIENT_MATCH_MODE_AND_STATE(c, mode, state))
            return c;

    /* if not found then find the first one matching from the head */
    for (Client *c = client->monitor->head; c && c != client; c = c->next)
        if (CLIENT_MATCH_MODE_AND_STATE(c, mode, state))
            return c;

    return NULL;
}

Client *client_previous(Client *client, Mode mode, State state)
{
    /* find the first ancestor matching */
    for (Client *c = client->prev; c; c = c->prev)
        if (CLIENT_MATCH_MODE_AND_STATE(c, mode, state))
            return c;

    /* if not found then find the first one matching from the tail */
    for (Client *c = client->monitor->tail; c && c != client; c = c->prev)
        if (CLIENT_MATCH_MODE_AND_STATE(c, mode, state))
            return c;

    return NULL;
}

#undef CLIENT_MATCH_MODE_AND_STATE
