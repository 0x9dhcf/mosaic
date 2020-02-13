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
#include "log.h"
#include "mwm.h"
#include "monitor.h"

static int xcb_reply_contains_atom(xcb_get_property_reply_t *reply, xcb_atom_t atom);

int xcb_reply_contains_atom(xcb_get_property_reply_t *reply, xcb_atom_t atom)
{
    if (reply == NULL || xcb_get_property_value_length(reply) == 0)
        return 0;

    xcb_atom_t *atoms;
    if ((atoms = xcb_get_property_value(reply)) == NULL)
        return 0;

    for (int i = 0; i < xcb_get_property_value_length(reply) / (reply->format / 8); i++)
        if (atoms[i] == atom)
            return 1;

    return 0;
}

void client_initialize(Client *client, xcb_window_t window)
{
    client->window = window;
    client->t_x = client->f_x = -1;
    client->t_y = client->f_y = -1;
    client->t_width = client->f_width = 1;
    client->t_height = client->f_height = 1;
    client->border_width = g_border_width;
    client->border_color = g_normal_color;
    client->state = STATE_TILED;
    client->saved_state = STATE_TILED;
    client->properties = PROPERTY_FOCUSABLE | PROPERTY_RESIZABLE;
    client->reserved_top = 0;
    client->reserved_bottom = 0;
    client->reserved_left = 0;
    client->reserved_right = 0;
    client->base_width = 0;
    client->base_height = 0;
    client->width_increment = 0;
    client->height_increment = 0;
    client->min_width = 0;
    client->min_height = 0;
    client->max_width = 0;
    client->max_height = 0;
    client->min_aspect_ratio = 0;
    client->max_aspect_ratio = 0;
    client->transient = 0;
    client->transient = XCB_NONE;
    client->monitor = NULL;
    client->tagset = -1;
    client->next = NULL;
    client->prev = NULL;

    xcb_change_save_set(g_xcb, XCB_SET_MODE_INSERT, window);

    /* manage the geometry of the window */
    // TODO: xcb calls could be //lized
    xcb_get_geometry_reply_t *geometry =
            xcb_get_geometry_reply(
                    g_xcb,
                    xcb_get_geometry(g_xcb, window),
                    NULL);

    if (geometry) {
        client->t_x = client->f_x = geometry->x;
        client->t_y = client->f_x = geometry->y;
        client->t_width = client->f_width = geometry->width;
        client->t_height = client->f_height = geometry->height;
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

    if (transient && xcb_get_property_value_length(transient) != 0) {
        xcb_icccm_get_wm_transient_for_from_reply(&client->transient, transient);
        free(transient);
    }

    /* keep track of event f interrest */
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

    /* grab buttons, clicks should focus windows */
    xcb_void_cookie_t xcb_grab_button(xcb_connection_t *conn, uint8_t owner_events, xcb_window_t grab_window, uint16_t event_mask, uint8_t pointer_mode, uint8_t keyboard_mode, xcb_window_t confine_to, xcb_cursor_t cursor, uint8_t button, uint16_t modifiers);

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
                client->state = g_rules[i].state;
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
                    -(client->monitor->width),
                    -(client->monitor->height) });

    xcb_ungrab_pointer(g_xcb, XCB_CURRENT_TIME);
}

void client_show(Client *client)
{
    if (IS_VISIBLE(client)) {
        int x, y, w, h;
        if (client->state == STATE_TILED) {
            x = client->t_x;
            y = client->t_y;
            w = client->t_width;
            h = client->t_height;
        } else if (client->state == STATE_FLOATING) {
            if (client->transient) {
                Client *t = lookup(client->transient);
                if (t) { /* transient for might be closed yet */
                    if (t->state == STATE_TILED) {
                        client->f_x = (t->t_x + t->t_width / 2) - client->f_width / 2;
                        client->f_y = (t->t_y + t->t_height / 2) - client->f_height / 2;
                    } else {
                        client->f_x = (t->f_x + t->f_width / 2) - client->f_width / 2;
                        client->f_y = (t->f_y + t->f_height / 2) - client->f_height / 2;
                    }
                }
            }
            x = client->f_x;
            y = client->f_y;
            w = client->f_width;
            h = client->f_height;
        } else { /* STATE_FULLSCREEN */
            x = client->monitor->x;
            y = client->monitor->y;
            w = client->monitor->width;
            h = client->monitor->height;
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
                XCB_CONFIG_WINDOW_BORDER_WIDTH |
                XCB_CONFIG_WINDOW_STACK_MODE,
                (const int []) {
                    x, y,
                    w, h,
                    client->border_width,
                    client->state == STATE_TILED ?
                            XCB_STACK_MODE_BELOW : XCB_STACK_MODE_ABOVE });

        xcb_ungrab_pointer(g_xcb, XCB_CURRENT_TIME);
    }
}

void client_apply_size_hints(Client *client)
{
    if (client->state == STATE_FULLSCREEN)
        return;

    /* Handle the size aspect ratio */
    double dx = client->f_width - client->base_width;
    double dy = client->f_height - client->base_height;
    double ratio = dx / dy;
    if (client->max_aspect_ratio > 0 && client->min_aspect_ratio > 0 && ratio > 0) {
        if (ratio < client->min_aspect_ratio) {
            dy = dx / client->min_aspect_ratio + 0.5;
            client->f_width  = dx + client->base_width;
            client->f_height = dy + client->base_height;
        } else if (ratio > client->max_aspect_ratio) {
            dx = dy * client->max_aspect_ratio + 0.5;
            client->f_width  = dx + client->base_width;
            client->f_height = dy + client->base_height;
        }
    }

    /* Handle the minimum size */
    client->f_width = MAX(client->f_width, client->min_width);
    client->f_height = MAX(client->f_height, client->min_height);

    /* Handle the maximum size */
    if (client->max_width > 0) {
        client->f_width = MIN(client->f_width, client->max_width);
    }
    if (client->max_height > 0) {
        client->f_height = MIN(client->f_height, client->max_height);
    }

    /* Handle the size increment */
    if (client->width_increment > 0 && client->height_increment > 0) {
        int t1 = client->f_width, t2 = client->f_height;
        t1 = client->base_width > t1 ? 0 : t1 - client->base_width;
        t2 = client->base_height > t2 ? 0 : t2 - client->base_height;
        client->f_width -= t1 % client->width_increment;
        client->f_height -= t2 % client->height_increment;
    }
}

int client_update_reserved(Client *client)
{
    client->reserved_top = 0;
    client->reserved_bottom = 0;
    client->reserved_left = 0;
    client->reserved_right = 0;

    xcb_ewmh_wm_strut_partial_t strut;
    if (xcb_ewmh_get_wm_strut_partial_reply(
            &g_ewmh,
            xcb_ewmh_get_wm_strut_partial(&g_ewmh, client->window),
            &strut,
            NULL) == 1) {
        client->reserved_top = strut.top;
        client->reserved_bottom = strut.bottom;
        client->reserved_left = strut.left;
        client->reserved_right = strut.right;
        return 1;
    }
    return 0;
}

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

    client->properties |= PROPERTY_RESIZABLE;
    client->base_width = client->base_height = 0;
    client->width_increment = client->height_increment = 0;
    client->max_width = client->max_height = 0;
    client->min_width = client->min_height = 0;
    client->max_aspect_ratio = client->min_aspect_ratio = 0.0;

    xcb_size_hints_t size;
    if (xcb_icccm_get_wm_size_hints_from_reply(&size, normal_hints)) {

        /* base size */
        if (size.flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE) {
            client->base_width = size.base_width;
            client->base_height = size.base_height;
        } else {
            /* note: not using min size as fallback */
            client->base_width = client->base_height = 0;
        }

        /* max size */
        if (size.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) {
            client->max_width = MAX(size.max_width, 1);
            client->max_height = MAX(size.max_height, 1);
        } else {
            client->max_width = client->max_height = UINT32_MAX;
        }

        /* min size */
        if (size.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
            client->min_width = size.min_width;
            client->min_height = size.min_height;
        } else {
            /* according to ICCCM 4.1.23 base size
             * should be used as a fallback */
            client->min_width = client->base_width;
            client->min_height = client->base_height;
        }

        /* increments */
        if (size.flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC) {
            client->width_increment = size.width_inc;
            client->height_increment = size.height_inc;
        } else {
            client->width_increment = client->height_increment = 1;
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
        client->max_aspect_ratio = (double)max_aspect_num / (double)max_aspect_den;
        client->min_aspect_ratio = (double)min_aspect_num / (double)min_aspect_den;

        if (client->max_width && client->max_height &&
                client->max_width == client->min_width &&
                client->max_height == client->min_height)
            client->properties &= ~PROPERTY_RESIZABLE;
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

    client->properties |= PROPERTY_FOCUSABLE;
    client->properties &= ~PROPERTY_URGENT;

    if (xcb_get_property_value_length(hints) != 0) {
        xcb_icccm_wm_hints_t wm_hints;
        if (xcb_icccm_get_wm_hints_from_reply(&wm_hints, hints)) {
            if (wm_hints.flags & XCB_ICCCM_WM_HINT_INPUT && ! wm_hints.input)
                client->properties &= ~PROPERTY_FOCUSABLE;
            if (xcb_icccm_wm_hints_get_urgency(&wm_hints))
                client->properties |= PROPERTY_URGENT;
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

    if (client->state != STATE_FLOATING &&
            (xcb_reply_contains_atom(type, g_ewmh._NET_WM_WINDOW_TYPE_DIALOG) ||
             xcb_reply_contains_atom(type, g_ewmh._NET_WM_WINDOW_TYPE_UTILITY) ||
             xcb_reply_contains_atom(type, g_ewmh._NET_WM_WINDOW_TYPE_TOOLBAR) ||
             xcb_reply_contains_atom(type, g_ewmh._NET_WM_WINDOW_TYPE_SPLASH))) {
        client->state = STATE_FLOATING;
        refresh = 1;
    }

    if (xcb_reply_contains_atom(type, g_ewmh._NET_WM_WINDOW_TYPE_DOCK)) {
        client->state = STATE_FLOATING;
        client->properties &= ~PROPERTY_FOCUSABLE;
        client->properties &= ~PROPERTY_RESIZABLE;
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
    if (focus && (client->monitor->layout == LT_NONE || client->state != STATE_TILED))
        xcb_configure_window (
                g_xcb,
                client->window,
                XCB_CONFIG_WINDOW_STACK_MODE,
                (const unsigned int[]) { XCB_STACK_MODE_ABOVE });
}

void client_set_fullscreen(Client *client, int fullscreen)
{
    if (fullscreen) {
        client->border_width = 0;
        client->saved_state = client->state;
        client->state = STATE_FULLSCREEN;
        client->saved_tagset = client->tagset;
        client->tagset = 0; /* visible on all monitor's tags */
    } else {
        client->state = client->saved_state;
        client->tagset = client->saved_tagset;
        client->border_width = g_border_width;
    }
}

void client_set_urgent(Client *client, int urgent)
{
    if (urgent)
        client->properties |= PROPERTY_URGENT;
    else
        client->properties &= ~PROPERTY_URGENT;

    xcb_change_window_attributes(
            g_xcb,
            client->window,
            XCB_CW_BORDER_PIXEL,
            (unsigned int []) { urgent ?  g_urgent_color : g_normal_color });
}

#define CLIENT_MATCH_STATE_AND_PROPERTIES(c, s, p)\
        ((s == STATE_ANY || c->state == s) && HAS_PROPERTIES(c, p) && IS_VISIBLE(c))

Client *client_next(Client *client, State state, Property properties)
{
    /* find the first successor matching */
    for (Client *c = client->next; c; c = c->next)
        if (CLIENT_MATCH_STATE_AND_PROPERTIES(c, state, properties))
            return c;

    /* if not found then find the first one matching from the head */
    for (Client *c = client->monitor->head; c && c != client; c = c->next)
        if (CLIENT_MATCH_STATE_AND_PROPERTIES(c, state, properties))
            return c;

    return NULL;
}

Client *client_previous(Client *client, State state, Property properties)
{
    /* find the first ancestor matching */
    for (Client *c = client->prev; c; c = c->prev)
        if (CLIENT_MATCH_STATE_AND_PROPERTIES(c, state, properties))
            return c;

    /* if not found then find the first one matching from the tail */
    for (Client *c = client->monitor->tail; c && c != client; c = c->prev)
        if (CLIENT_MATCH_STATE_AND_PROPERTIES(c, state, properties))
            return c;

    return NULL;
}

#undef CLIENT_MATCH_STATE_AND_PROPERTIES
