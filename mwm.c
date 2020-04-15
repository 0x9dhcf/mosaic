/*
 * Copyright (c) 2019, 2020 Pierre Evenou
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
#include <unistd.h>
#include <time.h>
#include <locale.h>
#include <getopt.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xkb.h>
#include <xcb/randr.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-x11.h>

#include "mwm.h"
#include "log.h"
#include "monitor.h"
#include "client.h"
#include "hints.h"
#include "events.h"
#include "settings.h"

/* static functions */
static void setup_xcb();
static void setup_atoms();
static void setup_window_manager();
static void setup_shortcuts();
static void add_monitor(Monitor *monitor);
static void del_monitor(Monitor *monitor);
static void check_focus_consistency();
static void cleanup_window_manager();
static void trap();
static void usage();
static void version();
static unsigned int parse_color(const char* hex);
static void swap(Client *c1, Client *c2);

/* static variables */
static const char *atom_names[MWM_ATOM_COUNT] = {
    "WM_TAKE_FOCUS",
    "MWM_MONITOR_TAGS",
    "MWM_MONITOR_TAGSET",
    "MWM_FOCUSED",
    "MWM_FOCUSED_TAGSET",
};

static xcb_window_t supporting_window = XCB_NONE;
static struct xkb_context *xkb_context = NULL;
static struct xkb_keymap *xkb_keymap = NULL;

static Monitor *monitor_head = NULL;
static Monitor *monitor_tail = NULL;
static Monitor *primary_monitor = NULL;
static Monitor *focused_monitor = NULL;
static Client *focused_client = NULL;

/* TODO
static xcb_atom_t wm_state_atom;
static xcb_atom_t wm_delete_window_atom;
*/

static int running;

void setup_xcb()
{
    g_xcb = xcb_connect(0, &g_screen_id);
    if (xcb_connection_has_error(g_xcb))
        FATAL("can't get xcb connection.");

    xcb_prefetch_extension_data(g_xcb, &xcb_xkb_id);
    xcb_prefetch_extension_data(g_xcb, &xcb_randr_id);

    xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(g_xcb));
    for (int i = 0; i < g_screen_id; ++i)
        xcb_screen_next(&it);
    g_screen = it.data;
    g_root = g_screen->root;
    const xcb_query_extension_reply_t *ext_reply;
    ext_reply = xcb_get_extension_data(g_xcb, &xcb_xkb_id);
    if (!ext_reply->present)
        FATAL("no xkb extension on this server.");
    ext_reply = xcb_get_extension_data(g_xcb, &xcb_randr_id);
    if (!ext_reply->present)
        FATAL("no randr extension on this server.");
}

void setup_atoms()
{
    xcb_intern_atom_cookie_t cookies[MWM_ATOM_COUNT];
    for (int i = 0; i < MWM_ATOM_COUNT; ++i)
        cookies[i] = xcb_intern_atom(
                g_xcb,
                0,
                strlen(atom_names[i]),
                atom_names[i]);

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

void setup_window_manager()
{
    /* check if a window manager is already running */
    xcb_void_cookie_t checkwm = xcb_change_window_attributes_checked(
            g_xcb,
            g_root,
            XCB_CW_EVENT_MASK,
            (int[]) {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT});

    xcb_intern_atom_cookie_t *init_atoms_cookie = xcb_ewmh_init_atoms(
            g_xcb,
            &g_ewmh);

    if (xcb_request_check(g_xcb, checkwm)) {
        free(init_atoms_cookie);
        FATAL("A windows manager is already running!\n");
    }

    if (! xcb_ewmh_init_atoms_replies(&g_ewmh, init_atoms_cookie, NULL))
        free(init_atoms_cookie);

    /* create the supporting window */
    supporting_window = xcb_generate_id(g_xcb);
    xcb_create_window(
            g_xcb,
            XCB_COPY_FROM_PARENT,
            supporting_window,
            g_root,
            -1, -1, 1, 1, 0,
            XCB_WINDOW_CLASS_INPUT_ONLY,
            XCB_COPY_FROM_PARENT,
            XCB_CW_OVERRIDE_REDIRECT,
            (int[]) {1});
    pid_t wm_pid = getpid();
    xcb_ewmh_set_supporting_wm_check(
            &g_ewmh,
            supporting_window,
            supporting_window);
    xcb_ewmh_set_wm_name(
            &g_ewmh,
            supporting_window,
            strlen(WMNAME), WMNAME);
    xcb_ewmh_set_wm_pid(&g_ewmh, supporting_window, wm_pid);

    /* configure the root window */
    xcb_ewmh_set_supporting_wm_check(
            &g_ewmh,
            g_root,
            supporting_window);
    xcb_ewmh_set_wm_name(
            &g_ewmh,
            g_root,
            strlen(WMNAME), WMNAME);
    xcb_ewmh_set_wm_pid(&g_ewmh, g_root, wm_pid);
    xcb_change_window_attributes(
            g_xcb,
            g_root,
            XCB_CW_EVENT_MASK,
            (const int []) {
                XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                XCB_EVENT_MASK_ENTER_WINDOW |
                XCB_EVENT_MASK_LEAVE_WINDOW |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                XCB_EVENT_MASK_KEY_PRESS |
                XCB_EVENT_MASK_KEY_RELEASE |
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_BUTTON_RELEASE |
                XCB_EVENT_MASK_FOCUS_CHANGE |
                XCB_EVENT_MASK_PROPERTY_CHANGE });

    /* reset the client list */
    xcb_delete_property(
            g_xcb,
            g_root,
            g_ewmh._NET_CLIENT_LIST);

    /* let ewmh listeners know about what is supported */
    xcb_change_property(
            g_xcb,
            XCB_PROP_MODE_REPLACE,
            g_root,
            g_ewmh._NET_SUPPORTED,
            XCB_ATOM_ATOM, 32,
            9, (xcb_atom_t[]) {
                g_ewmh._NET_ACTIVE_WINDOW,
                g_ewmh._NET_SUPPORTED,
                g_ewmh._NET_WM_NAME,
                g_ewmh._NET_WM_STATE,
                g_ewmh._NET_SUPPORTING_WM_CHECK,
                g_ewmh._NET_WM_STATE_FULLSCREEN,
                g_ewmh._NET_WM_WINDOW_TYPE,
                g_ewmh._NET_WM_WINDOW_TYPE_DIALOG,
                g_ewmh._NET_CLIENT_LIST
            });

    /* setting up keyborad and listen changes */
    /* TODO: setup a handler. */
    unsigned char xkb_base_event;
    xkb_x11_setup_xkb_extension(
            g_xcb,
            XKB_X11_MIN_MAJOR_XKB_VERSION,
            XKB_X11_MIN_MINOR_XKB_VERSION,
            0,
            NULL,
            NULL,
            &xkb_base_event,
            NULL);
    xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    int device = xkb_x11_get_core_keyboard_device_id(g_xcb);
    xkb_keymap = xkb_x11_keymap_new_from_device(
            xkb_context,
            g_xcb,
            device,
            XKB_KEYMAP_COMPILE_NO_FLAGS);
    g_xkb_state = xkb_x11_state_new_from_device(
            xkb_keymap,
            g_xcb,
            device);
    int map = XCB_XKB_EVENT_TYPE_STATE_NOTIFY |
              XCB_XKB_EVENT_TYPE_MAP_NOTIFY |
              XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY;
    int parts = XCB_XKB_MAP_PART_KEY_TYPES |
                XCB_XKB_MAP_PART_KEY_SYMS |
                XCB_XKB_MAP_PART_MODIFIER_MAP |
                XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS |
                XCB_XKB_MAP_PART_KEY_ACTIONS |
                XCB_XKB_MAP_PART_KEY_BEHAVIORS |
                XCB_XKB_MAP_PART_VIRTUAL_MODS |
                XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP;
    xcb_xkb_select_events(
            g_xcb,
            XCB_XKB_ID_USE_CORE_KBD,
            map,
            0,
            map,
            parts,
            parts,
            0);

    /* listen for input changes */
    xcb_randr_select_input(
            g_xcb,
            g_root,
            XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE |
            XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE |
            XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE |
            XCB_RANDR_NOTIFY_MASK_OUTPUT_PROPERTY);

    /* manage existing windows */
    xcb_query_tree_reply_t *tree = xcb_query_tree_reply(
            g_xcb,
            xcb_query_tree(g_xcb, g_root),
            NULL);

    if (tree) {
        int nb_children;
        xcb_window_t *children;
        xcb_get_window_attributes_cookie_t *ac;
        children = xcb_query_tree_children(tree);
        nb_children = xcb_query_tree_children_length(tree);
        ac = malloc(nb_children * sizeof(xcb_get_window_attributes_cookie_t));
        for (int i = 0; i < nb_children; i++)
            ac[i] = xcb_get_window_attributes(g_xcb, children[i]);

        for (int i = 0; i < nb_children; i++) {
            xcb_get_window_attributes_reply_t  *attributes;
            attributes = xcb_get_window_attributes_reply(
                    g_xcb,
                    ac[i],
                    NULL);
            if (attributes->override_redirect ||
                    attributes->map_state != XCB_MAP_STATE_VIEWABLE) {
                free(attributes);
                continue;
            }
            free(attributes);
            manage(children[i]);
        }
        free(ac);
        free(tree);
    }
}

void setup_shortcuts()
{
    xcb_ungrab_key(g_xcb, XCB_GRAB_ANY, g_root, XCB_MOD_MASK_ANY);
    xcb_key_symbols_t *ks = xcb_key_symbols_alloc(g_xcb);
    int i = 0;
    while (g_shortcuts[i].callback.vcb != NULL) {
        xcb_keycode_t *kcs;
        xcb_keycode_t *kc;

        kcs = xcb_key_symbols_get_keycode(ks, g_shortcuts[i].sequence.keysym);
        for (kc = kcs; *kc != XCB_NO_SYMBOL; kc++)
            xcb_grab_key(
                    g_xcb,
                    1,
                    g_root,
                    g_shortcuts[i].sequence.modifier,
                    *kc,
                    XCB_GRAB_MODE_ASYNC,
                    XCB_GRAB_MODE_ASYNC);
        i++;
    }
    xcb_key_symbols_free(ks);
}

void add_monitor(Monitor *monitor)
{
    if (monitor_tail) {
        monitor->prev = monitor_tail;
        monitor_tail->next = monitor;
    }

    if (! monitor_head)
        monitor_head = monitor;

    monitor_tail = monitor;
}

void del_monitor(Monitor *monitor)
{
    if (monitor->prev)
        monitor->prev->next = monitor->next;
    else
        monitor_head = monitor->next;

    if (monitor->next)
        monitor->next->prev = monitor->prev;
    else
        monitor_tail = monitor->prev;
}

void check_focus_consistency()
{
    if (focused_client &&
            focused_client->monitor == focused_monitor &&
            IS_VISIBLE(focused_client) &&
            IS_CLIENT_STATE(focused_client, STATE_ACCEPT_FOCUS))
        return; /* everything is fine with the focus */

    /* focus the first focusable client of the focused monitor */
    Client *f = focused_monitor->head;

    if (f && (! IS_VISIBLE(f) || ! IS_CLIENT_STATE(f, STATE_ACCEPT_FOCUS)))
        f = client_next(f, MODE_ANY, STATE_ACCEPT_FOCUS);

    if (! f || (! IS_VISIBLE(f) || ! IS_CLIENT_STATE(f, STATE_ACCEPT_FOCUS))) {
        focused_client = NULL;
        xcb_set_input_focus(
                g_xcb,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                g_root,
                XCB_CURRENT_TIME);
    } else {
        xcb_set_input_focus(
                g_xcb,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                f->window,
                XCB_CURRENT_TIME);
    }
    xcb_flush(g_xcb);
}

void cleanup_window_manager()
{
    xcb_aux_sync(g_xcb);
    xcb_grab_server(g_xcb);

    Monitor *m, *n;
    m = monitor_head;
    n = NULL;
    while (m) {
        Client *c, *d;
        n = m->next;

        c = m->head;
        d = NULL;
        while (c) {
            d = c->next;
            free(c);
            c = d;
        }
        free(m);
        m = n;
    }

    xcb_destroy_window(g_xcb, supporting_window);
    xcb_ungrab_server(g_xcb);
}

static void update_monitors(Monitor *scanned)
{
    for (Monitor *ms = scanned; ms; ms = ms->next)
        for (Monitor *me = monitor_head; me; me = me->next)
            if (strcmp(me->name, ms->name) == 0)
                me = ms;
}

static void add_new_monitors(Monitor *scanned)
{
    for (Monitor *ms = scanned; ms; ms = ms->next) {

        /* check if already exists */
        int exists = 0;
        for (Monitor *me = monitor_head; me; me = me->next) {
            if (strcmp(me->name, ms->name) == 0) {
                exists = 1;
                break;
            }
        }

        if (exists)
            continue;

        /* if not add it */
        INFO("Adding monitor %s: (%d, %d), [%d, %d]",
                ms->name,
                ms->geometry.x,
                ms->geometry.y,
                ms->geometry.width,
                ms->geometry.height);
        Monitor *m = malloc(sizeof(Monitor));
        monitor_initialize(
                m,
                ms->name,
                ms->geometry.x,
                ms->geometry.y,
                ms->geometry.width,
                ms->geometry.height);

        add_monitor(m);
    }
}

static void del_old_monitors(Monitor *scanned)
{
    Monitor *m, *n;
    m = monitor_head;
    while (m) {
         /* check if already exists */
        int exists = 0;
        for (Monitor *ms = scanned; ms; ms = ms->next) {
            if (strcmp(m->name, ms->name) == 0) {
                exists = 1;
                break;
            }
        }

        if (exists) {
            m = m->next;
            continue;
        }

        /* if not remove it */
        INFO("Removing monitor %s: (%d, %d), [%d, %d]",
                m->name,
                m->geometry.x,
                m->geometry.y,
                m->geometry.width,
                m->geometry.height);

        Client *c, *d;
        c = m->head;
        while (c) {
            d = c->next;
            monitor_detach(m, c);
            monitor_attach(primary_monitor, c);
            c = d;
        }

        n = m->next;
        del_monitor(m);
        free(m);
        m = n;
    }
}

void scan_monitors()
{
    Monitor *scanned = NULL;

    /* build the list of detected monitors */
    xcb_randr_get_monitors_reply_t *monitors_reply;
    monitors_reply = xcb_randr_get_monitors_reply(
            g_xcb,
            xcb_randr_get_monitors(g_xcb, g_root, 1),
            NULL);

    char primary_name[128];
    for (xcb_randr_monitor_info_iterator_t iter =
            xcb_randr_get_monitors_monitors_iterator(monitors_reply);
            iter.rem; xcb_randr_monitor_info_next(&iter)) {
        const xcb_randr_monitor_info_t *monitor_info = iter.data;

        xcb_get_atom_name_reply_t *name =
                xcb_get_atom_name_reply(
                        g_xcb,
                        xcb_get_atom_name(g_xcb, monitor_info->name),
                        NULL);
        /* XXX: name = NULL !!! */
        int len = xcb_get_atom_name_name_length(name);
        char cname[128];
        strncpy(cname, xcb_get_atom_name_name(name), len);
        cname[len] = '\0';
        Monitor *m = malloc(sizeof(Monitor));
        monitor_initialize(
                m,
                cname,
                monitor_info->x,
                monitor_info->y,
                monitor_info->width,
                monitor_info->height);

        /* add the monitor */
        if (scanned) {
            scanned->prev = m;
            m->next = scanned;
        }
        scanned = m;

        /* keep track of the primary */
        if (monitor_info->primary)
            strcpy(primary_name, cname);
    }
    free(monitors_reply);

    update_monitors(scanned);
    add_new_monitors(scanned);
    del_old_monitors(scanned);

    /* free the scanned list */
    Monitor *ms = scanned;
    while (ms) {
        Monitor *n = ms;
        ms = ms->next;
        free(n);
    }

    /* find the primay */
    for (Monitor *m = monitor_head; m; m = m->next) {
        if (strcmp(m->name, primary_name) == 0)
            primary_monitor = m;
    }

    /* if no monitor, fallback to root window */
    if (! monitor_head) {
        xcb_get_geometry_reply_t *reply =
                xcb_get_geometry_reply(
                        g_xcb,
                        xcb_get_geometry(g_xcb, g_root),
                        NULL);
        Monitor *m = malloc(sizeof(Monitor));
        monitor_initialize(
                m,
                "Default",
                reply->x,
                reply->y,
                reply->width,
                reply->height);
        /* add the monitor */
        if (monitor_tail) {
            m->prev = monitor_tail;
            monitor_tail->next = m;
        }

        if (! monitor_head)
            monitor_head = m;

        monitor_tail = m;
        free(reply);
    }

    /* set the primary */
    if (!primary_monitor)
        primary_monitor = monitor_head;

    focused_client = NULL;
    focused_monitor = primary_monitor;

    check_focus_consistency();
    hints_set_monitor(focused_monitor);

    /* render the monitors with updated geometry */
    for (Monitor *m = monitor_head; m; m = m->next)
        monitor_render(m, GS_CHANGED);
    xcb_flush(g_xcb);
}

void trap(int sig)
{
    quit();
}

void usage()
{
    printf("minimal window manager %s\n", VERSION);
    printf("Usage: mwm [OPTIONS]\n\n"\
           "-h, --help\t\tprint this message.\n"\
           "-v, --version\t\tprint the version.\n"\
           "-b, --border-width\tset window border width (default 1).\n"
           "-n, --normal-color\tset window border color (default grey).\n"
           "-f, --focused-color\tset window border color when focused (default blue).\n"
           "-u, --urgent-color\tset window border color when urgent (default red).\n");
    exit(2);
}

void version()
{
    printf("%s\n", VERSION);
    exit(EXIT_SUCCESS);
}

unsigned int parse_color(const char* hex)
{
    if (hex[0] != '#' || strlen(hex) != 7) {
        INFO("wrong color format: %s", hex);
        return 0;
    }
    return (unsigned int)strtoul(hex+1, NULL, 16);
}

void quit()
{
    running = 0;
    /* send the focus to the root window to trigger the loop event */
    /* XXX: does not trigger the loop??? */
    xcb_set_input_focus(
            g_xcb,
            XCB_INPUT_FOCUS_POINTER_ROOT,
            g_root,
            XCB_CURRENT_TIME);
    xcb_flush(g_xcb);
}

void dump()
{
    FILE *f = fopen("/tmp/mwm.dump", "w");
    if (!f)
        return;

    fprintf(f, "Screen: [%d x %d]\n",
            g_screen->width_in_pixels,
            g_screen->height_in_pixels);

    for (Monitor *m = monitor_head; m; m = m->next) {
        fprintf(f, "Monitor %s: (%d, %d) [%d, %d]\n",
                m->name,
                m->geometry.x, m->geometry.y,
                m->geometry.width, m->geometry.height);
        for (Client *c = m->head; c; c = c->next) {
            fprintf(f, "\t %p: %s, %d\n", c, (char*[]) {"tiled", "floating"}[c->mode - 1], c->state);
            fprintf(f, "\t\ttiled: (%d, %d) [%d, %d]\n",
                    c->tiling_geometry.x, c->tiling_geometry.y,
                    c->tiling_geometry.width, c->tiling_geometry.height);
            fprintf(f, "\t\tfloating: (%d, %d) [%d, %d]\n",
                    c->floating_geometry.x, c->floating_geometry.y,
                    c->floating_geometry.width, c->floating_geometry.height);
        }
    }
    fclose(f);
}

void manage(xcb_window_t window)
{
    /* create the client */
    Client *c = malloc(sizeof(Client));
    client_initialize(c, window);

    /* map it, attach it, focus it */
    xcb_map_window(g_xcb, c->window);
    if (c->transient) {
        Client *t = lookup(c->transient);
        if (t) {
            monitor_attach(t->monitor, c);
            monitor_render(t->monitor, GS_UNCHANGED);
        }
    } else if (IS_CLIENT_STATE(c, STATE_STICKY)) {
        monitor_attach(primary_monitor, c);
        monitor_render(primary_monitor, GS_UNCHANGED);
        hints_set_monitor(primary_monitor);
    } else {
        monitor_attach(focused_monitor, c);
        monitor_render(focused_monitor, GS_UNCHANGED);
        hints_set_monitor(focused_monitor);
    }

    if (IS_CLIENT_STATE(c, STATE_ACCEPT_FOCUS))
        xcb_set_input_focus(
                g_xcb,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                window,
                XCB_CURRENT_TIME);

    xcb_change_property(
            g_xcb,
            XCB_PROP_MODE_APPEND,
            g_root,
            g_ewmh._NET_CLIENT_LIST,
            XCB_ATOM_WINDOW, 32, 1, &window);

    xcb_flush(g_xcb);
    xcb_aux_sync(g_xcb);
}

Client * lookup(xcb_window_t window)
{
    if (window == g_root)
        return NULL;

    for (Monitor *m = monitor_head; m; m = m->next)
        for (Client *c = m->head; c; c = c->next)
            if (c->window == window)
                return c;

    return NULL;
}

void forget(xcb_window_t window)
{
    Client *c = lookup(window);

    if (!c)
        return;

    if (c == focused_client) {
        Client *f = NULL;
        if (c == c->monitor->head)
            f = client_next(c, MODE_ANY, STATE_ACCEPT_FOCUS);
        else
           f = client_previous(c, MODE_ANY, STATE_ACCEPT_FOCUS);

        if (f)
            xcb_set_input_focus(
                    g_xcb,
                    XCB_INPUT_FOCUS_POINTER_ROOT,
                    f->window,
                    XCB_CURRENT_TIME);
        else
            focused_client = NULL;
    }

    Monitor *m = c->monitor;
    monitor_detach(c->monitor, c);
    monitor_render(m, GS_UNCHANGED);
    free(c);

    xcb_delete_property(g_xcb, g_root, g_ewmh._NET_CLIENT_LIST);
    for (Monitor *m = monitor_head; m; m = m->next)
        for (Client *it = m->head; it; it = it->next)
            xcb_change_property(
                    g_xcb,
                    XCB_PROP_MODE_APPEND,
                    g_root,
                    g_ewmh._NET_CLIENT_LIST,
                    XCB_ATOM_WINDOW, 32, 1, &it->window);

    /* we did not flush yet for the consistency to be properly checked */
    if (! focused_client)
        check_focus_consistency();
    hints_set_monitor(focused_monitor);
    hints_set_focused(focused_client);
    xcb_flush(g_xcb);
}

/* to be called only by focus_in event ???
 * it does not set the "X" input focus */
void focus(Client *client)
{
    if (client == focused_client && client->monitor == focused_monitor)
        return;

    if (client == focused_client && client->monitor != focused_monitor) {
        focused_monitor = client->monitor;
        return;
    }

    focused_client = client;
    focused_monitor = client->monitor;
    client_set_focused(client, 1);
    xcb_ewmh_set_active_window(&g_ewmh, g_screen_id, client->window);

    xcb_client_message_event_t e;
    e.type = XCB_CLIENT_MESSAGE;
    e.window = client->window;
    e.type = g_ewmh.WM_PROTOCOLS;
    e.format = 32;
    e.data.data32[0] = g_atoms[WM_TAKE_FOCUS];
    e.data.data32[1] = XCB_CURRENT_TIME;
    xcb_send_event(
            g_xcb,
            0,
            client->window,
            XCB_EVENT_MASK_NO_EVENT,
            (char*)&e);
    hints_set_focused(focused_client);
    hints_set_monitor(focused_monitor);
    xcb_flush(g_xcb);
}

void unfocus(Client *client) {
    focused_client = NULL;
    client_set_focused(client, 0);
    hints_set_focused(focused_client);
    xcb_flush(g_xcb);
}

void focus_next_client()
{
    if (! focused_client)
        return;

    Client *c = client_next(focused_client, MODE_ANY, STATE_ACCEPT_FOCUS);
    if (c) {
        xcb_set_input_focus(
                g_xcb,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                c->window,
                XCB_CURRENT_TIME);

    }
    check_focus_consistency();
    xcb_flush(g_xcb);
}

void focus_previous_client()
{
    if (! focused_client)
        return;

    Client *c = client_previous(focused_client, MODE_ANY, STATE_ACCEPT_FOCUS);
    if (c) {
        xcb_set_input_focus(
                g_xcb,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                c->window,
                XCB_CURRENT_TIME);

    }
    check_focus_consistency();
    xcb_flush(g_xcb);
}

void focus_next_monitor()
{
    if (focused_monitor->next) {
        focused_monitor = focused_monitor->next;
        check_focus_consistency();
        hints_set_monitor(focused_monitor);
        hints_set_focused(focused_client);
        xcb_flush(g_xcb);
    }
}

void focus_previous_monitor()
{
    if (focused_monitor->prev) {
        focused_monitor = focused_monitor->prev;
        check_focus_consistency();
        hints_set_monitor(focused_monitor);
        hints_set_focused(focused_client);
        xcb_flush(g_xcb);
    }
}

void focus_clicked_monitor(int x, int y)
{
    for (Monitor *m = monitor_head; m; m = m->next) {
        if (x > m->geometry.x && x < m->geometry.x + m->geometry.width &&
                y > m->geometry.y && y < m->geometry.y + m->geometry.height) {
            focused_monitor = m;
            check_focus_consistency();
            hints_set_monitor(focused_monitor);
            hints_set_focused(focused_client);
            return;
        }
    }
}


void focused_monitor_update_main_views(int by)
{
    monitor_update_main_views(focused_monitor, by);
    monitor_render(focused_monitor, GS_UNCHANGED);
    xcb_flush(g_xcb);
}

void focused_monitor_set_layout(Layout layout)
{
    if (focused_monitor->layout != layout) {
        focused_monitor->layout = layout;
        monitor_render(focused_monitor, GS_UNCHANGED);
        xcb_flush(g_xcb);
    }
}

void focused_monitor_rotate_clockwise()
{
    if (! focused_monitor->tail)
        return;

    /* find the last tilable */
    Client *c = focused_monitor->tail->mode == MODE_TILED ?
            focused_monitor->tail :
            client_previous(focused_monitor->tail, MODE_TILED, STATE_ANY);

    if (!c || c == focused_monitor->head)
        return;

    /* move it to the head */
    if (c->prev) c->prev->next = c->next;
    if (c->next) c->next->prev = c->prev;
    if (! c->next) focused_monitor->tail = c->prev;
    c->prev = NULL;
    c->next = focused_monitor->head;
    focused_monitor->head->prev = c;
    focused_monitor->head = c;

    monitor_render(focused_monitor, GS_UNCHANGED);
    xcb_flush(g_xcb);
}

void focused_monitor_rotate_counter_clockwise()
{
    if (! focused_monitor->head)
        return;

    /* find the first tilable */
    Client *c = focused_monitor->head->mode == MODE_TILED ?
            focused_monitor->head :
            client_next(focused_monitor->head, MODE_TILED, STATE_ANY);

    if (!c || c == focused_monitor->tail)
        return;

    /* move it to the tail */
    if (c->prev) c->prev->next = c->next;
    if (c->next) c->next->prev = c->prev;
    if (! c->prev) focused_monitor->head = c->next;
    c->next = NULL;
    c->prev = focused_monitor->tail;
    focused_monitor->tail->next = c;
    focused_monitor->tail = c;

    monitor_render(focused_monitor, GS_UNCHANGED);
    xcb_flush(g_xcb);
}

/* set this tag and this tag only */
void focused_monitor_set_tag(int tag)
{
    focused_monitor->tagset = (1L << (tag - 1));
    check_focus_consistency();
    monitor_render(focused_monitor, GS_UNCHANGED);
    hints_set_monitor(focused_monitor);
    xcb_flush(g_xcb);
}

/* add or remove this tag */
void focused_monitor_toggle_tag(int tag)
{
    if (focused_monitor->tagset & (1L << (tag - 1)))
        focused_monitor->tagset &= ~(1L << (tag - 1));
    else
        focused_monitor->tagset |= (1L << (tag - 1));

    check_focus_consistency();
    monitor_render(focused_monitor, GS_UNCHANGED);
    hints_set_monitor(focused_monitor);
    xcb_flush(g_xcb);
}

void focused_client_kill()
{
    if (! focused_client)
        return;

    xcb_kill_client(g_xcb, focused_client->window);
    xcb_flush(g_xcb);
}

void focused_client_toggle_mode()
{
    if (! focused_client)
        return;

    if (focused_client->mode ==  MODE_FULLSCREEN ||
            IS_CLIENT_STATE(focused_client, STATE_STICKY))
        return;

    focused_client->mode = focused_client->mode == MODE_FLOATING ?
            MODE_TILED : MODE_FLOATING;

    monitor_render(focused_client->monitor, GS_UNCHANGED);
    xcb_flush(g_xcb);
}

/* we swap the content only and keep the list structure */
void swap(Client *c1, Client *c2) {
    Client t;

#define CLIENT_COPY_CONTENT(c1 , c2)\
    (c2)->window = (c1)->window;\
    (c2)->mode = (c1)->mode;\
    (c2)->border_width = (c1)->border_width;\
    (c2)->border_color = (c1)->border_color;\
    (c2)->state = (c1)->state;\
    (c2)->strut = (c1)->strut;\
    (c2)->size_hints = (c1)->size_hints;\
    (c2)->monitor = (c1)->monitor;\
    (c2)->tagset = (c1)->tagset;

    CLIENT_COPY_CONTENT(c1, &t);
    CLIENT_COPY_CONTENT(c2, c1);
    CLIENT_COPY_CONTENT(&t, c2);

#undef CLIENT_COPY_CONTENT

    client_show(c1);
    client_show(c2);

    /* swap the focus */
    focused_client = c1 == focused_client ? c2 : c1;
}

#define MOVE_INC 35

void focused_client_move(Direction direction)
{
    if (! focused_client || focused_client->mode  == MODE_FULLSCREEN)
        return;

    if (focused_client->mode == MODE_FLOATING) {
        switch (direction) {
            case D_UP:
                focused_client->floating_geometry.y -= MOVE_INC;
                break;
            case D_DOWN:
                focused_client->floating_geometry.y += MOVE_INC;
                break;
            case D_LEFT:
                focused_client->floating_geometry.x -= MOVE_INC;
                break;
            case D_RIGHT:
                focused_client->floating_geometry.x += MOVE_INC;
                break;
        }
        client_show(focused_client);
    } else {
        Client *c = NULL;
        switch (direction) {
            case D_UP:
            case D_LEFT:
                c = client_previous(focused_client, MODE_TILED, STATE_ANY);
                break;
            case D_DOWN:
            case D_RIGHT:
                c = client_next(focused_client, MODE_TILED, STATE_ANY);
                break;
        }
        if (c)
            swap(focused_client, c);
    }

    xcb_flush(g_xcb);
}

void focused_client_to_next_monitor()
{
    if (! focused_client)
        return;

    if (focused_client->monitor->next) {
        Client *c = focused_client;
        Monitor *cm = c->monitor;
        Monitor *nm = c->monitor->next;
        monitor_detach(cm, c);
        monitor_attach(nm, c);
        focused_monitor = c->monitor;
        check_focus_consistency();
        monitor_render(cm, GS_UNCHANGED);
        monitor_render(nm, GS_UNCHANGED);
        xcb_flush(g_xcb);
    }
}

void focused_client_to_previous_monitor()
{
    if (! focused_client)
        return;

    if (focused_client->monitor->prev) {
        Client *c = focused_client;
        Monitor *cm = c->monitor;
        Monitor *pm = c->monitor->prev;
        monitor_detach(cm, c);
        monitor_attach(pm, c);
        focused_monitor = c->monitor;
        check_focus_consistency();
        monitor_render(cm, GS_UNCHANGED);
        monitor_render(pm, GS_UNCHANGED);
        xcb_flush(g_xcb);
    }
}

#define MAIN_SPLIT_MIN .2
#define MAIN_SPLIT_MAX .8
#define MAIN_SPLIT_INC .05

void focused_client_resize(int width, int height)
{
    DEBUG("resize: %d, %d", width, height);
    if (! focused_client)
        return;

    if (focused_client->mode == MODE_FLOATING) {
        focused_client->floating_geometry.width += width;
        focused_client->floating_geometry.height += height;
        client_apply_size_hints(focused_client);
        client_show(focused_client);
        xcb_flush(g_xcb);
    } else {
        if ((width < 0 || height < 0) && focused_monitor->split > MAIN_SPLIT_MIN) {
            focused_monitor->split -= MAIN_SPLIT_INC;
            monitor_render(focused_monitor, GS_UNCHANGED);
            xcb_flush(g_xcb);
        }
        if ((width > 0 || height > 0) && focused_monitor->split < MAIN_SPLIT_MAX) {
            focused_monitor->split += MAIN_SPLIT_INC;
            monitor_render(focused_monitor, GS_UNCHANGED);
            xcb_flush(g_xcb);
        }
    }
}

/* set this tag and this tag only */
void focused_client_set_tag(int tag)
{
    /* do not change tagset of fullscreen client */
    if (! focused_client || focused_client->mode ==  MODE_FULLSCREEN)
        return;

    for (int i = 0; i < 32; ++i)
        if (focused_client->tagset & (1L << i))
            focused_monitor->tags[i]--;

    focused_client->tagset = (1L << (tag - 1));
    focused_client->tagset |= (1L << (tag - 1));
    focused_monitor->tags[tag - 1]++;

    monitor_render(focused_monitor, GS_UNCHANGED);
    check_focus_consistency();
    hints_set_focused(focused_client);
    hints_set_monitor(focused_monitor);

    xcb_flush(g_xcb);
}

/* add or remove this tag */
void focused_client_toggle_tag(int tag)
{
    /* do not change tagset of fullscreen client */
    if (! focused_client || focused_client->mode ==  MODE_FULLSCREEN)
        return;

    if (focused_client->tagset & (1L << (tag - 1))) {
        focused_client->tagset &= ~(1L << (tag - 1));
        if (! --focused_monitor->tags[tag - 1])
            focused_monitor->tagset &= ~(1L << (tag - 1));
    } else {
        focused_client->tagset |= (1L << (tag - 1));
        focused_monitor->tags[tag - 1]++;
    }

    if (focused_monitor->tags[tag - 1] == 0 ||
            focused_monitor->tagset & (1L << (tag - 1)))
        monitor_render(focused_monitor, GS_UNCHANGED);

    check_focus_consistency();
    hints_set_focused(focused_client);
    hints_set_monitor(focused_monitor);
    xcb_flush(g_xcb);
}


int main(int argc, char **argv)
{
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"border-width", required_argument, 0, 'b'},
        {"normal-color", required_argument, 0, 'n'},
        {"focused-color", required_argument, 0, 'f'},
        {"urgent-color", required_argument, 0, 'u'},
        {0, 0, 0, 0}};
    int option_index = 0, opt;

    setlocale(LC_ALL, "");

    while ((opt = getopt_long(argc, argv, "hvb:n:f:u", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                usage();
                break;
            case 'v':
                version();
                break;
            case 'b':
                g_border_width = atoi(optarg);
                break;
            case 'n':
                g_normal_color = parse_color(optarg);
                break;
            case 'f':
                g_focused_color = parse_color(optarg);
                break;
            case 'u':
                g_urgent_color = parse_color(optarg);
                break;
            default:
                usage();
        }
    }

    setup_xcb();
    setup_atoms();
    scan_monitors();
    setup_window_manager();
    setup_shortcuts();

    /* trap signals */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP,  SIG_IGN);
    signal(SIGINT,  trap);
    signal(SIGKILL, trap);
    signal(SIGTERM, trap);

#ifdef NDEBUG
    /* autostart */
    char *home = 0;
    char autostart[1024];
    struct stat st;
    autostart[0] = '\0';
    home = getenv("HOME");
    if (home) {
        snprintf(autostart, sizeof(autostart), "%s/%s", home, AUTOSTART);
        if (stat(autostart, &st) == 0 && st.st_mode & S_IXUSR) {
            INFO("execute: %s.", autostart);
            system(autostart);
        } else {
            INFO("no mwmrc found.");
        }
    }
#endif

    /* listen for events */
    INFO("entering main loop.");
    running = 1;
    while (running) {
        xcb_generic_event_t *event;
        while ((event = xcb_wait_for_event(g_xcb))) {
            if (! running)
                break;
            if (event->response_type == 0) {
                xcb_generic_error_t *e = (xcb_generic_error_t *)event;
                /* ignore some events */
                if (e->error_code != XCB_WINDOW
                        && (e->major_code != XCB_SET_INPUT_FOCUS &&
                            e->error_code == XCB_MATCH)
                        && (e->major_code != XCB_CONFIGURE_WINDOW &&
                            e->error_code == XCB_MATCH)
                        && (e->major_code != XCB_GRAB_BUTTON &&
                            e->error_code == XCB_ACCESS)
                        && (e->major_code != XCB_GRAB_KEY &&
                            e->error_code == XCB_ACCESS))
                    ERROR("X11 Error, sequence 0x%x, resource %d, code = %d\n",
                            e->sequence,
                            e->resource_id,
                            e->error_code);
                free(event);
                continue;
            }
            on_event(event);
            free(event);
        }
    }

    cleanup_window_manager();
    xcb_ewmh_connection_wipe(&g_ewmh);
    xcb_disconnect(g_xcb);

    return 0;
}
