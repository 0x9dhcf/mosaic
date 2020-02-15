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

#include <unistd.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xkb.h>
#include <xcb/xcb_keysyms.h>

#include "events.h"
#include "log.h"
#include "mwm.h"
#include "hints.h"

static void notify(Client *c);
static void on_configure_request(xcb_configure_request_event_t *e);
static void on_map_request(xcb_map_request_event_t *e);
static void on_unmap_notify(xcb_unmap_notify_event_t *e);
static void on_property_notify(xcb_property_notify_event_t *e);
static void on_focus_in(xcb_focus_in_event_t *e);
static void on_focus_out(xcb_focus_out_event_t *e);
static void on_enter_notify(xcb_enter_notify_event_t *e);
static void on_client_message(xcb_client_message_event_t *e);
static void on_button_press(xcb_button_press_event_t *e);
static void on_key_press(xcb_key_press_event_t *e);

void notify(Client *c)
{
    xcb_configure_notify_event_t event;
    event.event = c->window;
    event.window = c->window;
    event.response_type = XCB_CONFIGURE_NOTIFY;
    if (c->mode == MODE_TILED) {
        event.x = c->t_x;
        event.y = c->t_y;
        event.width = c->t_width;
        event.height = c->t_height;
    } else {
        event.x = c->f_x;
        event.y = c->f_y;
        event.width = c->f_width;
        event.height = c->f_height;
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

void on_configure_request(xcb_configure_request_event_t *e)
{
    Client *c = lookup(e->window);

    if (c) {
        if (c->mode == MODE_FLOATING) {
            if (e->value_mask & XCB_CONFIG_WINDOW_X)
                c->f_x = e->x;
            if (e->value_mask & XCB_CONFIG_WINDOW_Y)
                c->f_y = e->y;
            if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH)
                c->f_width = e->width;
            if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
                c->f_height = e->height;
            if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
                c->border_width = e->border_width;

            client_apply_size_hints(c);

            if (e->value_mask & (XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y) &&
                    !(e->value_mask & (XCB_CONFIG_WINDOW_WIDTH |
                            XCB_CONFIG_WINDOW_HEIGHT)))
                notify(c);

            if (IS_VISIBLE(c))
                xcb_configure_window(
                        g_xcb,
                        c->window,
                        XCB_CONFIG_WINDOW_X |
                        XCB_CONFIG_WINDOW_Y |
                        XCB_CONFIG_WINDOW_WIDTH |
                        XCB_CONFIG_WINDOW_HEIGHT |
                        XCB_CONFIG_WINDOW_BORDER_WIDTH,
                        (const int []) {
                            c->f_x,
                            c->f_y,
                            c->f_width,
                            c->f_height,
                            c->border_width });
        } else {
            /* Resend as notify */
            notify(c);
        }
    } else {
        xcb_configure_window(
                g_xcb,
                e->window,
                XCB_CONFIG_WINDOW_X |
                XCB_CONFIG_WINDOW_Y |
                XCB_CONFIG_WINDOW_WIDTH |
                XCB_CONFIG_WINDOW_HEIGHT |
                XCB_CONFIG_WINDOW_BORDER_WIDTH,
                (int[]) { e->x, e->y, e->width, e->height, e->border_width });
    }
    xcb_aux_sync(g_xcb);
}

/* TODO
static void on_configure_notify(xcb_configure_notify_event_t *e)
{
    if (e->window == g_root)
        scan_monitors();
}
*/

void on_map_request(xcb_map_request_event_t *e)
{
    if (lookup(e->window))
        return;

    xcb_get_window_attributes_reply_t *attributes;
    attributes = xcb_get_window_attributes_reply(
            g_xcb,
            xcb_get_window_attributes(g_xcb, e->window),
            NULL);

    if (attributes && ! attributes->override_redirect)
        manage(e->window);

    free(attributes);
}

/* TODO
static void on_mapping_notify(xcb_mapping_notify_event_t *e)
{
    // TODO: Keyboard Change
}
*/

void on_unmap_notify(xcb_unmap_notify_event_t *e)
{
    forget(e->window);
}

void on_property_notify(xcb_property_notify_event_t *e)
{
    int refresh = 0;

    if (e->state == XCB_PROPERTY_DELETE)
        return;

    Client *client = lookup(e->window);

    if (! client)
        return;

    switch(e->atom) {
        case XCB_ATOM_WM_NORMAL_HINTS:
            client_update_size_hints(client);
            break;
        case XCB_ATOM_WM_HINTS:
            client_update_wm_hints(client);
            break;
        default: break;
    }

    if (e->atom == g_ewmh._NET_WM_WINDOW_TYPE)
        if (client_update_window_type(client))
            refresh = 1;

    if (e->atom == g_ewmh._NET_WM_STRUT_PARTIAL)
        if (client_update_reserved(client))
            refresh = 1;

    if (refresh)
        monitor_render(client->monitor);
}

void on_focus_in(xcb_focus_in_event_t *e)
{
    /* ignore focus changes due to keyboard grabs */
    if (e->mode == XCB_NOTIFY_MODE_GRAB || e->mode == XCB_NOTIFY_MODE_UNGRAB)
      return;

    if (e->detail == XCB_NOTIFY_DETAIL_POINTER)
        return;

    Client *c = lookup(e->event);
    if (c && IS_VISIBLE(c) && IS_CLIENT_STATE(c, STATE_FOCUSABLE))
        focus(c);
}

void on_focus_out(xcb_focus_out_event_t *e)
{
    if (e->mode == XCB_NOTIFY_MODE_GRAB || e->mode == XCB_NOTIFY_MODE_UNGRAB)
      return;

    if (e->detail == XCB_NOTIFY_DETAIL_POINTER)
        return;

    Client *c = lookup(e->event);
    if (c)
        unfocus(c);
}

void on_enter_notify(xcb_enter_notify_event_t *e)
{
    if(e->mode != XCB_NOTIFY_MODE_NORMAL || e->event == g_root)
        return;

    Client *c = lookup(e->event);
    if (c && IS_CLIENT_STATE(c, STATE_FOCUSABLE)) {
        xcb_set_input_focus(
                g_xcb,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                c->window,
                XCB_CURRENT_TIME);
        xcb_flush(g_xcb);
    }
}

// NOT A SO BAD IDEA!!!!
//static void on_root_message(xcb_client_message_event_t *e)
//{
//    if (e->type == g_atoms[MWM_QUIT])
//        quit();
//
//    if (e->type == g_atoms[MWM_FOCUS_PREVIOUS_CLIENT])
//        focus_previous_client();
//    if (e->type == g_atoms[MWM_FOCUS_NEXT_CLIENT])
//        focus_next_client();
//    if (e->type == g_atoms[MWM_FOCUS_PREVIOUS_MONITOR])
//        focus_previous_monitor();
//    if (e->type == g_atoms[MWM_FOCUS_NEXT_MONITOR])
//        focus_next_monitor();
//
//    if (e->type == g_atoms[MWM_FOCUSED_MONITOR_INCREASE_MAIN_VIEWS])
//        focused_monitor_increase_main_views();
//    if (e->type == g_atoms[MWM_FOCUSED_MONITOR_INCREASE_MAIN_VIEWS])
//        focused_monitor_decrease_main_views();
//    if (e->type == g_atoms[MWM_FOCUSED_MONITOR_SET_LAYOUT])
//        focused_monitor_set_layout(e->data.data32[0]);
//    if (e->type == g_atoms[MWM_FOCUSED_MONITOR_SET_TAG])
//        focused_monitor_set_tag(e->data.data32[0]);
//    if (e->type == g_atoms[MWM_FOCUSED_MONITOR_TOGGLE_TAG])
//        focused_Monitoroggle_tag(e->data.data32[0]);
//
//    if (e->type == g_atoms[MWM_FOCUSED_CLIENT_KILL])
//        focused_client_kill();
//    if (e->type == g_atoms[MWM_FOCUSED_CLIENT_MOVE_UP])
//        focused_client_move_up();
//    if (e->type == g_atoms[MWM_FOCUSED_CLIENT_MOVE_DOWN])
//        focused_client_move_down();
//    if (e->type == g_atoms[MWM_FOCUSED_CLIENT_MOVE_LEFT])
//        focused_client_move_left();
//    if (e->type == g_atoms[MWM_FOCUSED_CLIENT_MOVE_RIGHT])
//        focused_client_move_right();
//    if (e->type == g_atoms[MWM_FOCUSED_CLIENT_TO_NEXT_MONITOR])
//        focused_Cliento_next_monitor();
//    if (e->type == g_atoms[MWM_FOCUSED_CLIENT_TO_PREVIOUS_MONITOR])
//        focused_Cliento_previous_monitor();
//    if (e->type == g_atoms[MWM_FOCUSED_CLIENT_DECREASE_WIDTH])
//        focused_client_decrease_width();
//    if (e->type == g_atoms[MWM_FOCUSED_CLIENT_INCREASE_WIDTH])
//        focused_client_increase_width();
//    if (e->type == g_atoms[MWM_FOCUSED_CLIENT_DECREASE_HEIGHT])
//        focused_client_decrease_height();
//    if (e->type == g_atoms[MWM_FOCUSED_CLIENT_INCREASE_HEIGHT])
//        focused_client_increase_height();
//    if (e->type == g_atoms[MWM_FOCUSED_CLIENT_SET_TAG])
//        focused_client_set_tag(e->data.data32[0]);
//    if (e->type == g_atoms[MWM_FOCUSED_CLIENT_TOGGLE_TAG])
//        focused_Clientoggle_tag(e->data.data32[0]);
//}

void on_client_message(xcb_client_message_event_t *e)
{
//    if (e->window == g_root) {
//        on_root_message(e);
//        return;
//    }

    /* TODO: STICKY */
    int refresh = 0;
    Client *c = lookup(e->window);
    if (! c)
        return;

    if (e->type == g_ewmh._NET_ACTIVE_WINDOW &&
            IS_CLIENT_STATE(c, STATE_FOCUSABLE))
        xcb_set_input_focus(
                g_xcb,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                c->window,
                XCB_CURRENT_TIME);

    if (e->type == g_ewmh._NET_WM_STATE) {
#define STATE(event, atom) (event->data.data32[1] == atom || event->data.data32[2] == atom)

        if (STATE(e, g_ewmh._NET_WM_STATE_FULLSCREEN)) {
            if (IS_CLIENT_STATE_NOT(c, STATE_FULLSCREEN)  &&
                    (e->data.data32[0] ==  XCB_EWMH_WM_STATE_ADD ||
                     e->data.data32[0] ==  XCB_EWMH_WM_STATE_TOGGLE))
                client_set_fullscreen(c, 1);
            if (IS_CLIENT_STATE(c, STATE_FULLSCREEN)  &&
                    (e->data.data32[0] ==  XCB_EWMH_WM_STATE_REMOVE ||
                     e->data.data32[0] ==  XCB_EWMH_WM_STATE_TOGGLE))
                client_set_fullscreen(c, 0);
        } else if (STATE(e,  g_ewmh._NET_WM_STATE_DEMANDS_ATTENTION)) {
            /* Check if the urgent flag must be set */
            if (e->data.data32[0] == XCB_EWMH_WM_STATE_ADD)
                client_set_urgency(c, 1);
            else if (e->data.data32[0] == XCB_EWMH_WM_STATE_REMOVE)
                client_set_urgency(c, 1);
            else if (e->data.data32[0] == XCB_EWMH_WM_STATE_TOGGLE)
                client_set_urgency(c, IS_CLIENT_STATE(c, STATE_URGENT));
        } else if (STATE(e,  g_ewmh._NET_WM_STATE_MODAL)) {
            if (c->mode != MODE_FLOATING  &&
                    (e->data.data32[0] ==  XCB_EWMH_WM_STATE_ADD ||
                     e->data.data32[0] ==  XCB_EWMH_WM_STATE_TOGGLE))
                c->mode = MODE_FLOATING;
            if (c->mode == MODE_FLOATING  &&
                    (e->data.data32[0] ==  XCB_EWMH_WM_STATE_REMOVE ||
                     e->data.data32[0] ==  XCB_EWMH_WM_STATE_TOGGLE))
                /* XXX: who said the client was not floating before
                 * being set to modal */
                c->mode = MODE_TILED;
        }
#undef STATE

        int count = 0;
        xcb_atom_t atoms[2];

        if (IS_CLIENT_STATE(c, STATE_FULLSCREEN))
            atoms[count++] = g_ewmh._NET_WM_STATE_FULLSCREEN;
        if (IS_CLIENT_STATE(c, STATE_URGENT))
            atoms[count++] = g_ewmh._NET_WM_STATE_DEMANDS_ATTENTION;

        xcb_ewmh_set_wm_state(&g_ewmh, c->window, count, atoms);

        refresh = 1;
    }

    if (refresh)
        monitor_render(c->monitor);

    xcb_flush(g_xcb);
}

void on_button_press(xcb_button_press_event_t *e)
{
    /*if(e->event == g_root)
        return;*/

    Client *c = lookup(e->event);
    if (c && IS_CLIENT_STATE(c, STATE_FOCUSABLE))
        xcb_set_input_focus(
                g_xcb,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                c->window,
                XCB_CURRENT_TIME);

    xcb_allow_events(g_xcb, XCB_ALLOW_REPLAY_POINTER, XCB_CURRENT_TIME);
    xcb_flush(g_xcb);
}

//static void on_keyboard_state_change(xcb_xkb_state_notify_event_t *e)
//{
//    /* Just keep track of modifiers for now.
//     * keyboard or keymap changes etc.. see i3 or i3lock */
//    xkb_state_update_mask(
//            g_xkb_state,
//            e->baseMods,
//            e->latchedMods,
//            e->lockedMods,
//            e->baseGroup,
//            e->latchedGroup,
//            e->lockedGroup);
//}

void on_key_press(xcb_key_press_event_t *e)
{
    xkb_keycode_t keycode = e->detail;
    xkb_keysym_t keysym = xkb_state_key_get_one_sym(g_xkb_state, keycode);

    int i = 0;
    while (g_shortcuts[i].callback.vcb != NULL) {
        if (g_shortcuts[i].sequence.modifier == e->state &&
                g_shortcuts[i].sequence.keysym == keysym) {
            switch (g_shortcuts[i].type) {
                case CB_VOID:
                    g_shortcuts[i].callback.vcb();
                    break;
                case CB_INT:
                    g_shortcuts[i].callback.icb(g_shortcuts[i].arg.i);
                    break;
            }
        }
        i++;
    }
}

void on_event(xcb_generic_event_t *event)
{
    switch(event->response_type & ~0x80) {
        case XCB_CONFIGURE_REQUEST:
            on_configure_request((xcb_configure_request_event_t*)event);
            break;
        //case XCB_CONFIGURE_NOTIFY:
        //    on_configure_notify((xcb_configure_notify_event_t*)event);
        //    break;
        case XCB_MAP_REQUEST:
            on_map_request((xcb_map_request_event_t *)event);
            break;
        //case XCB_MAPPING_NOTIFY:
        //    on_mapping_notify((xcb_mapping_notify_event_t *)event);
        //    break;
        case XCB_UNMAP_NOTIFY:
            on_unmap_notify((xcb_unmap_notify_event_t *)event);
            break;
        case XCB_PROPERTY_NOTIFY:
            on_property_notify((xcb_property_notify_event_t *)event);
            break;
        //case XCB_EXPOSE:
        //    on_expose((xcb_expose_event_t *)event);
        //    break;
        case XCB_FOCUS_IN:
            on_focus_in((xcb_focus_in_event_t *)event);
            break;
        case XCB_FOCUS_OUT:
            on_focus_out((xcb_focus_out_event_t *)event);
            break;
        case XCB_ENTER_NOTIFY:
            on_enter_notify((xcb_enter_notify_event_t *)event);
            break;
        case XCB_CLIENT_MESSAGE:
            on_client_message((xcb_client_message_event_t *)event);
            break;
        case XCB_BUTTON_PRESS:
            on_button_press((xcb_button_press_event_t *)event);
            break;
        //case XCB_BUTTON_RELEASE:
        //    on_button_release((xcb_button_press_event_t *)event);
        //    break;
        case XCB_KEY_PRESS:
            on_key_press((xcb_key_press_event_t *)event);
            break;
        //case XCB_KEY_RELEASE:
        //    on_key_release((xcb_key_press_event_t *)event);
        //    break;
        //case XCB_MOTION_NOTIFY:
        //    on_motion_notify((xcb_motion_notify_event_t *)event);
        //    break;
        //case XCB_DESTROY_NOTIFY:
        //    on_destroy_notify((xcb_destroy_notify_event_t *)event);
        //    break;
        //case XCB_RANDR_SCREEN_CHANGE_NOTIFY:
        //    scan_monitors();
        //    break;
    }
}