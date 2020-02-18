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
    client->mode = MODE_TILED;
    client->saved_mode = MODE_TILED;
    client->state = STATE_FOCUSABLE;
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
        xcb_icccm_get_wm_transient_for_from_reply(
                &client->transient,
                transient);
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
}

void client_set_focusable(Client *client, int focusable)
{
    if (focusable)
        CLIENT_SET_STATE(client, STATE_FOCUSABLE);
    else
        CLIENT_UNSET_STATE(client, STATE_FOCUSABLE);
}

void client_set_sticky(Client *client, int sticky)
{
    if (sticky) {
        CLIENT_SET_STATE(client, STATE_STICKY);
        client->mode = MODE_FLOATING;
    } else {
        CLIENT_UNSET_STATE(client, STATE_STICKY);
        /* what should be the state now ??? */
    }
}

void client_set_fullscreen(Client *client, int fullscreen)
{
    if (fullscreen) {
        client->border_width = 0;
        client->saved_mode = client->mode;
        client->saved_tagset = client->tagset;
        client->tagset = 0; /* visible on all monitor's tags */
        CLIENT_SET_STATE(client, STATE_FULLSCREEN);
    } else {
        client->mode = client->saved_mode;
        client->tagset = client->saved_tagset;
        client->border_width = g_border_width;
        CLIENT_UNSET_STATE(client, STATE_FULLSCREEN);
    }
}

void client_set_urgency(Client *client, int urgency)
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
                    -(client->monitor->width),
                    -(client->monitor->height) });

    xcb_ungrab_pointer(g_xcb, XCB_CURRENT_TIME);
}

void client_show(Client *client)
{
    if (IS_VISIBLE(client)) {
        int x, y, w, h;
        if (IS_CLIENT_STATE(client, STATE_FULLSCREEN)) {
            x = client->monitor->x;
            y = client->monitor->y;
            w = client->monitor->width;
            h = client->monitor->height;
        } else {
            if (client->mode == MODE_TILED) {
                x = client->t_x;
                y = client->t_y;
                w = client->t_width;
                h = client->t_height;
            } else { /* MODE_FLOATING */
                if (client->transient) {
                    Client *t = lookup(client->transient);
                    if (t) { /* transient for might be closed yet */
                        if (t->mode == MODE_TILED) {
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
            }
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
                    client->mode == MODE_FLOATING ||
                            IS_CLIENT_STATE(client, STATE_FULLSCREEN) ?
                            XCB_STACK_MODE_ABOVE : XCB_STACK_MODE_BELOW });

        xcb_ungrab_pointer(g_xcb, XCB_CURRENT_TIME);
    }
}

void client_apply_size_hints(Client *client)
{
    if (IS_CLIENT_STATE(client, STATE_FULLSCREEN))
        return;

    /* handle the size aspect ratio */
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

    /* handle the minimum size */
    client->f_width = MAX(client->f_width, client->min_width);
    client->f_height = MAX(client->f_height, client->min_height);

    /* handle the maximum size */
    if (client->max_width > 0) {
        client->f_width = MIN(client->f_width, client->max_width);
    }
    if (client->max_height > 0) {
        client->f_height = MIN(client->f_height, client->max_height);
    }

    /* handle the size increment */
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

    client_set_sticky(client, 0);
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
            client_set_sticky(client, 1);
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

    client_set_focusable(client, 1);
    client_set_urgency(client, 0);

    if (xcb_get_property_value_length(hints) != 0) {
        xcb_icccm_wm_hints_t wm_hints;
        if (xcb_icccm_get_wm_hints_from_reply(&wm_hints, hints)) {
            if (wm_hints.flags & XCB_ICCCM_WM_HINT_INPUT && ! wm_hints.input)
                client_set_focusable(client, 0);
            if (xcb_icccm_wm_hints_get_urgency(&wm_hints))
                client_set_urgency(client, 1);
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
        client->mode = MODE_FLOATING;
        client_set_focusable(client, 0);
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
