#include <unistd.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xkb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/randr.h>

#include "bar.h"
#include "client.h"
#include "events.h"
#include "hints.h"
#include "log.h"
#include "mosaic.h"
#include "settings.h"

static void on_expose(xcb_expose_event_t *e);
static void on_configure_request(xcb_configure_request_event_t *e);
static void on_configure_notify(xcb_configure_notify_event_t *e);
static void on_map_request(xcb_map_request_event_t *e);
static void on_unmap_notify(xcb_unmap_notify_event_t *e);
static void on_property_notify(xcb_property_notify_event_t *e);
static void on_focus_in(xcb_focus_in_event_t *e);
static void on_focus_out(xcb_focus_out_event_t *e);
static void on_enter_notify(xcb_enter_notify_event_t *e);
static void on_client_message(xcb_client_message_event_t *e);
static void on_button_press(xcb_button_press_event_t *e);
static void on_key_press(xcb_key_press_event_t *e);
static void spawn(char **argv);

void
on_configure_request(xcb_configure_request_event_t *e)
{
    Client *c = lookup(e->window);

    if (c) {
        if (c->mode == MODE_FLOATING) {
            if (e->value_mask & XCB_CONFIG_WINDOW_X)
                c->tiling_geometry.x = c->floating_geometry.x = e->x;
            if (e->value_mask & XCB_CONFIG_WINDOW_Y)
                c->tiling_geometry.y = c->floating_geometry.y = e->y;
            if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH)
                c->tiling_geometry.width = c->floating_geometry.width = e->width;
            if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
                c->tiling_geometry.height = c->floating_geometry.height = e->height;
            if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
                c->border_width = e->border_width;

            client_apply_size_hints(c);

            if (e->value_mask & (XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y) &&
                    !(e->value_mask & (XCB_CONFIG_WINDOW_WIDTH |
                            XCB_CONFIG_WINDOW_HEIGHT)))
                client_notify(c);

            if (client_is_visible(c)) {
                int x = c->floating_geometry.x;
                int y = c->floating_geometry.y;
                int w = c->floating_geometry.width;
                int h = c->floating_geometry.height;

                // XXX tiling geometry really??
                if ((c->state & STATE_STICKY) == STATE_STICKY) {
                    x = c->monitor->geometry.x + c->tiling_geometry.x;
                    y = c->monitor->geometry.y + c->tiling_geometry.y;
                }
                xcb_configure_window(
                        g_xcb,
                        c->window,
                        XCB_CONFIG_WINDOW_X |
                        XCB_CONFIG_WINDOW_Y |
                        XCB_CONFIG_WINDOW_WIDTH |
                        XCB_CONFIG_WINDOW_HEIGHT |
                        XCB_CONFIG_WINDOW_BORDER_WIDTH,
                        (const int []) {x, y, w, h, c->border_width });
            }
        } else {
            /* Resend as notify */
            client_notify(c);
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

void
on_expose(xcb_expose_event_t *e)
{
    if (bar_is_window(e->window))
        refresh_wmstatus();
}

void
on_configure_notify(xcb_configure_notify_event_t *e)
{
    if (e->window == g_root)
        scan_monitors();
}

void
on_map_request(xcb_map_request_event_t *e)
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
    TODO: Keyboard Change
}
*/

void
on_unmap_notify(xcb_unmap_notify_event_t *e)
{
    forget(e->window);
}

void
on_property_notify(xcb_property_notify_event_t *e)
{
    int refresh = 0;

    if (e->state == XCB_PROPERTY_DELETE)
        return;

    /* root window */
    if (e->window == g_root && (e->atom == XCB_ATOM_WM_NAME)) {
        bar_display_systatus();
        xcb_flush(g_xcb);
        return;
    }

    /* client window */
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
        if (client_update_strut(client))
            refresh = 1;

    if (refresh) {
        monitor_render(client->monitor, GS_UNCHANGED);
        xcb_flush(g_xcb);
    }
}

void
on_focus_in(xcb_focus_in_event_t *e)
{
    /* ignore focus changes due to keyboard grabs */
    if (e->mode == XCB_NOTIFY_MODE_GRAB || e->mode == XCB_NOTIFY_MODE_UNGRAB)
      return;

    if (e->detail == XCB_NOTIFY_DETAIL_POINTER)
        return;

    if (e->event == g_root) {
        find_focus(0);
        refresh_wmstatus();
        xcb_flush(g_xcb);
        return;
    }

    Client *c = lookup(e->event);
    if (c && client_is_visible(c) &&
            (c->state & STATE_ACCEPT_FOCUS) == STATE_ACCEPT_FOCUS)
        focus(c);

    xcb_ewmh_set_active_window(&g_ewmh, g_screen_id, e->event);

    xcb_client_message_event_t evt;
    evt.type = XCB_CLIENT_MESSAGE;
    evt.window = e->event;
    evt.type = g_ewmh.WM_PROTOCOLS;
    evt.format = 32;
    evt.data.data32[0] = g_atoms[WM_TAKE_FOCUS];
    evt.data.data32[1] = XCB_CURRENT_TIME;
    xcb_send_event(
            g_xcb,
            0,
            evt.window,
            XCB_EVENT_MASK_NO_EVENT,
            (char*)&e);
    xcb_flush(g_xcb);
}

void
on_focus_out(xcb_focus_out_event_t *e)
{
    if (e->mode == XCB_NOTIFY_MODE_GRAB || e->mode == XCB_NOTIFY_MODE_UNGRAB)
      return;

    if (e->detail == XCB_NOTIFY_DETAIL_POINTER)
        return;

    Client *c = lookup(e->event);
    if (c)
        unfocus(c);
}

void
on_enter_notify(xcb_enter_notify_event_t *e)
{
    if(e->mode != XCB_NOTIFY_MODE_NORMAL || e->event == g_root)
        return;

    Client *c = lookup(e->event);
    client_set_input_focus(c);
    xcb_flush(g_xcb);
}

void
on_client_message(xcb_client_message_event_t *e)
{
    /* TODO: STICKY */
    int refresh = 0;
    Client *c = lookup(e->window);
    if (! c)
        return;

    if (e->type == g_ewmh._NET_ACTIVE_WINDOW)
        client_set_input_focus(c);

    if (e->type == g_ewmh._NET_WM_STATE) {
#define STATE(event, atom) (event->data.data32[1] == atom || event->data.data32[2] == atom)

        if (STATE(e, g_ewmh._NET_WM_STATE_FULLSCREEN)) {
            if (c->mode != MODE_FULLSCREEN &&
                    (e->data.data32[0] ==  XCB_EWMH_WM_STATE_ADD ||
                     e->data.data32[0] ==  XCB_EWMH_WM_STATE_TOGGLE))
                client_set_fullscreen(c, 1);
            if (c->mode ==  MODE_FULLSCREEN  &&
                    (e->data.data32[0] ==  XCB_EWMH_WM_STATE_REMOVE ||
                     e->data.data32[0] ==  XCB_EWMH_WM_STATE_TOGGLE))
                client_set_fullscreen(c, 0);
        } else if (STATE(e,  g_ewmh._NET_WM_STATE_DEMANDS_ATTENTION)) {
            if ((c->state & STATE_URGENT) != STATE_URGENT  &&
                    (e->data.data32[0] ==  XCB_EWMH_WM_STATE_ADD ||
                     e->data.data32[0] ==  XCB_EWMH_WM_STATE_TOGGLE))
                client_set_urgent(c, 1);
            if ((c->state & STATE_URGENT) == STATE_URGENT  &&
                    (e->data.data32[0] ==  XCB_EWMH_WM_STATE_REMOVE ||
                     e->data.data32[0] ==  XCB_EWMH_WM_STATE_TOGGLE))
                client_set_urgent(c, 0);
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
        } else if (STATE(e,  g_ewmh._NET_WM_STATE_STICKY)) {
            if ((c->state & STATE_STICKY) != STATE_STICKY  &&
                    (e->data.data32[0] ==  XCB_EWMH_WM_STATE_ADD ||
                     e->data.data32[0] ==  XCB_EWMH_WM_STATE_TOGGLE))
                client_set_sticky(c, 1);
            if ((c->state & STATE_STICKY) == STATE_STICKY  &&
                    (e->data.data32[0] ==  XCB_EWMH_WM_STATE_REMOVE ||
                     e->data.data32[0] ==  XCB_EWMH_WM_STATE_TOGGLE))
                client_set_sticky(c, 0);
        }
#undef STATE

        int count = 0;
        xcb_atom_t atoms[2];

        if (c->mode ==  MODE_FULLSCREEN)
            atoms[count++] = g_ewmh._NET_WM_STATE_FULLSCREEN;
        if ((c->state & STATE_URGENT) == STATE_URGENT)
            atoms[count++] = g_ewmh._NET_WM_STATE_DEMANDS_ATTENTION;

        xcb_ewmh_set_wm_state(&g_ewmh, c->window, count, atoms);

        refresh = 1;
    }

    if (refresh)
        monitor_render(c->monitor, GS_UNCHANGED);

    xcb_flush(g_xcb);
}

void
on_button_press(xcb_button_press_event_t *e)
{
    if(e->event == g_root) {
        focus_clicked_monitor(e->root_x, e->root_y);
        return;
    }

    if (bar_is_window(e->event)) {
        if (fork() == 0) {
            int pid = fork();
            if (pid == 0) {
                if (g_xcb)
                    close(xcb_get_file_descriptor(g_xcb));
                setsid();
                system("uxterm");
                exit(EXIT_SUCCESS);
            }
            if (pid > 0)
                exit(EXIT_SUCCESS);
        }
    }

    Client *c = lookup(e->event);

    if (c)
        client_set_input_focus(c);
    //if (c && (c->state & STATE_ACCEPT_FOCUS) == STATE_ACCEPT_FOCUS
    //        && e->detail == XCB_BUTTON_INDEX_1)
    //    xcb_set_input_focus(
    //            g_xcb,
    //            XCB_INPUT_FOCUS_POINTER_ROOT,
    //            c->window,
    //            XCB_CURRENT_TIME);

    xcb_allow_events(g_xcb, XCB_ALLOW_REPLAY_POINTER, XCB_CURRENT_TIME);
    xcb_flush(g_xcb);
}

/* TODO
void
on_keyboard_state_change(xcb_xkb_state_notify_event_t *e)
{
     * Just keep track of modifiers for now.
     * keyboard or keymap changes etc.. see i3 or i3lock
    xkb_state_update_mask(
            g_xkb_state,
            e->baseMods,
            e->latchedMods,
            e->lockedMods,
            e->baseGroup,
            e->latchedGroup,
            e->lockedGroup);
}
*/

void
on_key_press(xcb_key_press_event_t *e)
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
                    g_shortcuts[i].callback.icb(g_shortcuts[i].args[0]);
                    break;
                case CB_INT_INT:
                    g_shortcuts[i].callback.iicb(
                            g_shortcuts[i].args[0],
                            g_shortcuts[i].args[1]);
                    break;
            }
        }
        i++;
    }

    i = 0;
    while (g_bindings[i].args[0] != NULL) {
        if (g_bindings[i].sequence.modifier == e->state &&
                g_bindings[i].sequence.keysym == keysym)
            spawn((char**)g_bindings[i].args);
        i++;
    }
}

void
spawn(char **argv)
{
    if (fork() == 0) {
        int pid = fork();
        if (pid == 0) {
            if (g_xcb)
                close(xcb_get_file_descriptor(g_xcb));
            setsid();
            execvp(argv[0], argv);
            exit(EXIT_SUCCESS);
        }
        if (pid > 0)
            exit(EXIT_SUCCESS);

    }
}

void
on_event(xcb_generic_event_t *event)
{
    switch(event->response_type & ~0x80) {
        case XCB_EXPOSE:
            on_expose((xcb_expose_event_t*)event);
            break;
        case XCB_CONFIGURE_REQUEST:
            on_configure_request((xcb_configure_request_event_t*)event);
            break;
        case XCB_CONFIGURE_NOTIFY:
            on_configure_notify((xcb_configure_notify_event_t*)event);
            break;
        case XCB_MAP_REQUEST:
            on_map_request((xcb_map_request_event_t *)event);
            break;
        /*
        case XCB_MAPPING_NOTIFY:
            on_mapping_notify((xcb_mapping_notify_event_t *)event);
            break;
        */
        case XCB_UNMAP_NOTIFY:
            on_unmap_notify((xcb_unmap_notify_event_t *)event);
            break;
        case XCB_PROPERTY_NOTIFY:
            on_property_notify((xcb_property_notify_event_t *)event);
            break;
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
        case XCB_KEY_PRESS:
            on_key_press((xcb_key_press_event_t *)event);
            break;
        /* Doesn't seem to be triggered?? */
        case XCB_RANDR_SCREEN_CHANGE_NOTIFY:
            scan_monitors();
            break;
    }
}
