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
#include <unistd.h>
#include <time.h>
#include <locale.h>
#include <signal.h>
#include <fcntl.h>
#include <getopt.h>
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

#define AUTOSTART ".mwmrc"

#define K_M         MODKEY
#define K_MC        MODKEY | XCB_MOD_MASK_CONTROL
#define K_MS        MODKEY | XCB_MOD_MASK_SHIFT
#define K_MCS       MODKEY | XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_SHIFT

/* static "extern" configuration */
unsigned int    g_border_width      = 1;
unsigned int    g_normal_color      = 0x888888;
unsigned int    g_focused_color     = 0x0000ff;
unsigned int    g_urgent_color      = 0xff0000;
//unsigned int    g_normal_color      = 0x333333;
//unsigned int    g_focused_color     = 0x888888;
//unsigned int    g_urgent_color      = 0xfb4934;

Shortcut g_shortcuts[] = {
    /* global */
    {{K_MS,     XKB_KEY_Home},          CB_VOID, {quit}, {0}},

    /* focus */
    {{K_M,      XKB_KEY_Left},      CB_VOID,    {focus_previous_client},                {}},
    {{K_M,      XKB_KEY_Up},        CB_VOID,    {focus_previous_client},                {}},
    {{K_M,      XKB_KEY_Right},     CB_VOID,    {focus_next_client},                    {}},
    {{K_M,      XKB_KEY_Down},      CB_VOID,    {focus_next_client},                    {}},
    {{K_M,      XKB_KEY_period},    CB_VOID,    {focus_next_monitor},                   {}},
    {{K_M,      XKB_KEY_comma},     CB_VOID,    {focus_previous_monitor},               {}},

    /* monitors */
    {{K_MC,     XKB_KEY_Page_Up},   CB_VOID,    {focused_monitor_increase_main_views},  {}},
    {{K_MC,     XKB_KEY_Page_Down}, CB_VOID,    {focused_monitor_decrease_main_views},  {}},
    {{K_MC,     XKB_KEY_space},     CB_INT,     {focused_monitor_set_layout},           {LT_NONE}},
    {{K_MC,     XKB_KEY_Right},     CB_INT,     {focused_monitor_set_layout},           {LT_RIGHT}},
    {{K_MC,     XKB_KEY_Left},      CB_INT,     {focused_monitor_set_layout},           {LT_LEFT}},
    {{K_MC,     XKB_KEY_Down},      CB_INT,     {focused_monitor_set_layout},           {LT_BOTTOM}},
    {{K_MC,     XKB_KEY_Up},        CB_INT,     {focused_monitor_set_layout},           {LT_TOP}},
    {{K_M,      XKB_KEY_1},         CB_INT,     {focused_monitor_set_tag},              {1}},
    {{K_M,      XKB_KEY_2},         CB_INT,     {focused_monitor_set_tag},              {2}},
    {{K_M,      XKB_KEY_3},         CB_INT,     {focused_monitor_set_tag},              {3}},
    {{K_M,      XKB_KEY_4},         CB_INT,     {focused_monitor_set_tag},              {4}},
    {{K_M,      XKB_KEY_5},         CB_INT,     {focused_monitor_set_tag},              {5}},
    {{K_M,      XKB_KEY_6},         CB_INT,     {focused_monitor_set_tag},              {6}},
    {{K_M,      XKB_KEY_7},         CB_INT,     {focused_monitor_set_tag},              {7}},
    {{K_M,      XKB_KEY_8},         CB_INT,     {focused_monitor_set_tag},              {8}},
    {{K_M,      XKB_KEY_9},         CB_INT,     {focused_monitor_set_tag},              {9}},
    {{K_MC,     XKB_KEY_1},         CB_INT,     {focused_monitor_toggle_tag},           {1}},
    {{K_MC,     XKB_KEY_2},         CB_INT,     {focused_monitor_toggle_tag},           {2}},
    {{K_MC,     XKB_KEY_3},         CB_INT,     {focused_monitor_toggle_tag},           {3}},
    {{K_MC,     XKB_KEY_4},         CB_INT,     {focused_monitor_toggle_tag},           {4}},
    {{K_MC,     XKB_KEY_5},         CB_INT,     {focused_monitor_toggle_tag},           {5}},
    {{K_MC,     XKB_KEY_6},         CB_INT,     {focused_monitor_toggle_tag},           {6}},
    {{K_MC,     XKB_KEY_7},         CB_INT,     {focused_monitor_toggle_tag},           {7}},
    {{K_MC,     XKB_KEY_8},         CB_INT,     {focused_monitor_toggle_tag},           {8}},
    {{K_MC,     XKB_KEY_9},         CB_INT,     {focused_monitor_toggle_tag},           {9}},

    /* focused client */
    {{K_MS,     XKB_KEY_q},         CB_VOID,    {focused_client_kill},                  {}},
    {{K_MCS,    XKB_KEY_space},     CB_VOID,    {focused_client_toggle_mode},           {}},
    {{K_MS,     XKB_KEY_Up},        CB_VOID,    {focused_client_move_up},               {}},
    {{K_MS,     XKB_KEY_Down},      CB_VOID,    {focused_client_move_down},             {}},
    {{K_MS,     XKB_KEY_Left},      CB_VOID,    {focused_client_move_left},             {}},
    {{K_MS,     XKB_KEY_Right},     CB_VOID,    {focused_client_move_right},            {}},
    {{K_MS,     XKB_KEY_period},    CB_VOID,    {focused_client_to_next_monitor},       {}},
    {{K_MS,     XKB_KEY_comma },    CB_VOID,    {focused_client_to_previous_monitor},   {}},
    {{K_MCS,    XKB_KEY_Up},        CB_VOID,    {focused_client_decrease_height},       {}},
    {{K_MCS,    XKB_KEY_Down},      CB_VOID,    {focused_client_increase_height},       {}},
    {{K_MCS,    XKB_KEY_Left},      CB_VOID,    {focused_client_decrease_width},        {}},
    {{K_MCS,    XKB_KEY_Right},     CB_VOID,    {focused_client_increase_width},        {}},
    {{K_MS,     XKB_KEY_1},         CB_INT,     {focused_client_set_tag},               {1}},
    {{K_MS,     XKB_KEY_2},         CB_INT,     {focused_client_set_tag},               {2}},
    {{K_MS,     XKB_KEY_3},         CB_INT,     {focused_client_set_tag},               {3}},
    {{K_MS,     XKB_KEY_4},         CB_INT,     {focused_client_set_tag},               {4}},
    {{K_MS,     XKB_KEY_5},         CB_INT,     {focused_client_set_tag},               {5}},
    {{K_MS,     XKB_KEY_6},         CB_INT,     {focused_client_set_tag},               {6}},
    {{K_MS,     XKB_KEY_7},         CB_INT,     {focused_client_set_tag},               {7}},
    {{K_MS,     XKB_KEY_8},         CB_INT,     {focused_client_set_tag},               {8}},
    {{K_MS,     XKB_KEY_9},         CB_INT,     {focused_client_set_tag},               {9}},
    {{K_MCS,    XKB_KEY_1},         CB_INT,     {focused_client_toggle_tag},            {1}},
    {{K_MCS,    XKB_KEY_2},         CB_INT,     {focused_client_toggle_tag},            {2}},
    {{K_MCS,    XKB_KEY_3},         CB_INT,     {focused_client_toggle_tag},            {3}},
    {{K_MCS,    XKB_KEY_4},         CB_INT,     {focused_client_toggle_tag},            {4}},
    {{K_MCS,    XKB_KEY_5},         CB_INT,     {focused_client_toggle_tag},            {5}},
    {{K_MCS,    XKB_KEY_6},         CB_INT,     {focused_client_toggle_tag},            {6}},
    {{K_MCS,    XKB_KEY_7},         CB_INT,     {focused_client_toggle_tag},            {7}},
    {{K_MCS,    XKB_KEY_8},         CB_INT,     {focused_client_toggle_tag},            {8}},
    {{K_MCS,    XKB_KEY_9},         CB_INT,     {focused_client_toggle_tag},            {9}},

    {{ 0, 0 }, 0, {NULL}, {0}}
};

Rule g_rules[] = {
    /* class                instance            TAGSET      State */
    { "Gnome-calculator",   "gnome-calculor",   -1,         MODE_FLOATING },
    { "Xephyr",             "Xephyr",           -1,         MODE_FLOATING },
    { "Xmessage",           "xmessage",         -1,         MODE_FLOATING },
    { NULL, NULL, 0, 0 }
};

/* static functions */
static void setup_xcb();
static void setup_atoms();
static void setup_window_manager();
static void setup_shortcuts();
static void check_focus_consistency();
static void cleanup_window_manager();
static void scan_monitors();
static void trap();
static void usage();
static void version();
static unsigned int parse_color(const char* hex);

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
static Monitor *Monitorail = NULL;
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
    /* setup the window manager */
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

void check_focus_consistency()
{
    if (focused_client &&
            focused_client->monitor == focused_monitor &&
            IS_VISIBLE(focused_client) &&
            IS_CLIENT_STATE(focused_client, STATE_FOCUSABLE))
        return; /* everything is fine with the focus */

    /* focus the first focusable client of the focused monitor */
    Client *f = focused_monitor->head;

    if (f && (! IS_VISIBLE(f) || ! IS_CLIENT_STATE(f, STATE_FOCUSABLE)))
        f = client_next(f, MODE_ANY, STATE_FOCUSABLE);

    if (! f || (! IS_VISIBLE(f) || ! IS_CLIENT_STATE(f, STATE_FOCUSABLE))) {
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

    Monitor *m = monitor_head;
    while (m) {
        Client *c = m->head;
        while (c) {
            Client *d = c;
            c = c->next;
            free (d);
        }
        Monitor *n = m;
        m = m->next;
        free(n);
    }

    xcb_destroy_window(g_xcb, supporting_window);
    xcb_ungrab_server(g_xcb);
}

void scan_monitors()
{
    xcb_randr_get_monitors_reply_t *monitors_reply;
    monitors_reply = xcb_randr_get_monitors_reply(
            g_xcb,
            xcb_randr_get_monitors(g_xcb, g_root, 1),
            NULL);

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
        if (Monitorail) {
            m->prev = Monitorail;
            Monitorail->next = m;
        }

        if (! monitor_head)
            monitor_head = m;

        Monitorail = m;
        free(name);

        if (monitor_info->primary)
            primary_monitor = m;
    }
    free(monitors_reply);

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
        if (Monitorail) {
            m->prev = Monitorail;
            Monitorail->next = m;
        }

        if (! monitor_head)
            monitor_head = m;

        Monitorail = m;
        free(reply);
    }

    /* set the primary */
    if (!primary_monitor)
        primary_monitor = monitor_head;

    focused_client = NULL;
    focused_monitor = primary_monitor;

    check_focus_consistency();
    hints_set_monitor(focused_monitor);
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
            monitor_render(t->monitor);
        }
    } else {
        monitor_attach(focused_monitor, c);
        monitor_render(focused_monitor);
        hints_set_monitor(focused_monitor);
    }

    if (IS_CLIENT_STATE(c, STATE_FOCUSABLE))
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

    if (c == focused_client)
        focused_client = NULL;

    Monitor *m = c->monitor;
    monitor_detach(c->monitor, c);
    monitor_render(m);
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
    xcb_flush(g_xcb);
}

void unfocus(Client *client) {
    focused_client = NULL;
    client_set_focused(client, 0);
    hints_set_focused(focused_client);
    xcb_flush(g_xcb);
}

#define CHECK_FOCUSED if (! focused_client) return;

void focus_next_client()
{
    CHECK_FOCUSED

    Client *c = client_next(focused_client, MODE_ANY, STATE_FOCUSABLE);
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
    CHECK_FOCUSED

    Client *c = client_previous(focused_client, MODE_ANY, STATE_FOCUSABLE);
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
    }
}

void focus_previous_monitor()
{
    if (focused_monitor->prev) {
        focused_monitor = focused_monitor->prev;
        check_focus_consistency();
    }
}

void focused_monitor_render()
{
    monitor_render(focused_monitor);
    xcb_flush(g_xcb);
}

void focused_monitor_increase_main_views()
{
    if (monitor_increase_main_views(focused_monitor)) {
        monitor_render(focused_monitor);
        xcb_flush(g_xcb);
    }
}

void focused_monitor_decrease_main_views()
{
    if (monitor_decrease_main_views(focused_monitor)) {
        monitor_render(focused_monitor);
        xcb_flush(g_xcb);
    }
}

void focused_monitor_set_layout(Layout layout)
{
    if (focused_monitor->layout != layout) {
        focused_monitor->layout = layout;
        monitor_render(focused_monitor);
        xcb_flush(g_xcb);
    }
}

/* set this tag and this tag only */
void focused_monitor_set_tag(int tag)
{
    focused_monitor->tagset = (1L << (tag - 1));
    check_focus_consistency();
    monitor_render(focused_monitor);
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
    monitor_render(focused_monitor);
    hints_set_monitor(focused_monitor);
    xcb_flush(g_xcb);
}

void focused_client_render()
{
    CHECK_FOCUSED
    client_show(focused_client);
}

void focused_client_kill()
{
    CHECK_FOCUSED

    xcb_kill_client(g_xcb, focused_client->window);
    xcb_flush(g_xcb);
}

void focused_client_toggle_mode()
{
    CHECK_FOCUSED

    if (IS_CLIENT_STATE(focused_client, STATE_FULLSCREEN) ||
            IS_CLIENT_STATE(focused_client, STATE_STICKY))
        return;

    focused_client->mode = focused_client->mode == MODE_FLOATING ?
            MODE_TILED : MODE_FLOATING;

    monitor_render(focused_client->monitor);
    xcb_flush(g_xcb);
}


#define MOVE_INC 35

#define CLIENT_COPY(c1 , c2)\
    (c1)->window = (c2)->window;\
    (c1)->f_x = (c2)->t_x;\
    (c1)->f_y = (c2)->t_y;\
    (c1)->f_width = (c2)->t_width;\
    (c1)->f_height = (c2)->t_height;\
    (c1)->border_width = (c2)->border_width;\
    (c1)->border_color = (c2)->border_color;\
    (c1)->mode = (c2)->mode;\
    (c1)->saved_mode = (c2)->saved_mode;\
    (c1)->state = (c2)->state;\
    (c1)->reserved_top = (c2)->reserved_top;\
    (c1)->reserved_bottom = (c2)->reserved_bottom;\
    (c1)->reserved_left = (c2)->reserved_left;\
    (c1)->reserved_right = (c2)->reserved_right;\
    (c1)->base_width = (c2)->base_width;\
    (c1)->base_height = (c2)->base_height;\
    (c1)->width_increment = (c2)->width_increment;\
    (c1)->height_increment = (c2)->height_increment;\
    (c1)->min_width = (c2)->min_width;\
    (c1)->min_height = (c2)->min_height;\
    (c1)->max_width = (c2)->max_width;\
    (c1)->max_height = (c2)->max_height;\
    (c1)->min_aspect_ratio = (c2)->min_aspect_ratio;\
    (c1)->max_aspect_ratio = (c2)->max_aspect_ratio;\
    (c1)->transient = (c2)->transient;\
    (c1)->monitor = (c2)->monitor;\
    (c1)->tagset = (c2)->tagset;\

/* we swap the content only and keep the list structure */
static void swap(Client *c1, Client *c2) {
    Client t;

    CLIENT_COPY(&t, c1);
    CLIENT_COPY(c1, c2);
    CLIENT_COPY(c2, &t);

    client_show(c1);
    client_show(c2);

    /* swap the focus */
    focused_client = c1 == focused_client ? c2 : c1;
}

void focused_client_move_up()
{
    CHECK_FOCUSED

    if (focused_client->mode == MODE_FLOATING) {
        focused_client->f_y -= MOVE_INC;
        client_show(focused_client);
    } else {
        Client *c = client_previous(focused_client, MODE_TILED, STATE_ANY);
        if (c)
            swap(focused_client, c);
    }

    xcb_flush(g_xcb);
}

void focused_client_move_down()
{
    CHECK_FOCUSED

    if (focused_client->mode == MODE_FLOATING) {
        focused_client->f_y += MOVE_INC;
        client_show(focused_client);
    } else {
        Client *c = client_next(focused_client, MODE_TILED, STATE_ANY);
        if (c)
            swap(focused_client, c);
    }

    xcb_flush(g_xcb);
}

void focused_client_move_left()
{
    CHECK_FOCUSED

    if (focused_client->mode == MODE_FLOATING) {
        focused_client->f_x -= MOVE_INC;
        client_show(focused_client);
    } else {
        Client *c = client_previous(focused_client, MODE_TILED, STATE_ANY);
        if (c)
            swap(focused_client, c);
    }

    xcb_flush(g_xcb);
}

void focused_client_move_right()
{
    CHECK_FOCUSED

    if (focused_client->mode == MODE_FLOATING) {
        focused_client->f_x += MOVE_INC;
        client_show(focused_client);
    } else {
        Client *c = client_next(focused_client, MODE_TILED, STATE_ANY);
        if (c)
            swap(focused_client, c);
    }

    xcb_flush(g_xcb);
}

void focused_client_to_next_monitor()
{
    CHECK_FOCUSED

    if (focused_client->monitor->next) {
        Client *c = focused_client;
        Monitor *cm = c->monitor;
        Monitor *nm = c->monitor->next;
        monitor_detach(cm, c);
        monitor_attach(nm, c);
        focused_monitor = c->monitor;
        check_focus_consistency();
    }
}

void focused_client_to_previous_monitor()
{
    CHECK_FOCUSED

    if (focused_client->monitor->prev) {
        Client *c = focused_client;
        Monitor *cm = c->monitor;
        Monitor *pm = c->monitor->prev;
        monitor_detach(cm, c);
        monitor_attach(pm, c);
        focused_monitor = c->monitor;
        check_focus_consistency();
    }
}

#define MAIN_SPLIT_MIN .2
#define MAIN_SPLIT_MAX .8
#define MAIN_SPLIT_INC .05
#define SIZE_INC 10

void focused_client_increase_width()
{
    CHECK_FOCUSED

    if (focused_client->mode == MODE_FLOATING) {
        focused_client->f_width += SIZE_INC;
        client_apply_size_hints(focused_client); /* TODO: return if no change */
        client_show(focused_client);
        xcb_flush(g_xcb);
    } else {
        if (focused_monitor->split < MAIN_SPLIT_MAX) {
            focused_monitor->split += MAIN_SPLIT_INC;
            monitor_render(focused_monitor);
            xcb_flush(g_xcb);
        }
    }
}

void focused_client_decrease_width()
{
    CHECK_FOCUSED

    if (focused_client->mode == MODE_FLOATING) {
        focused_client->f_width -= SIZE_INC;
        client_apply_size_hints(focused_client); /* TODO: return if no change */
        client_show(focused_client);
        xcb_flush(g_xcb);
    } else {
        if (focused_monitor->split > MAIN_SPLIT_MIN) {
            focused_monitor->split -= MAIN_SPLIT_INC;
            monitor_render(focused_monitor);
            xcb_flush(g_xcb);
        }
    }
}

void focused_client_increase_height()
{
    CHECK_FOCUSED

    if (focused_client->mode == MODE_FLOATING) {
        focused_client->f_height += SIZE_INC;
        client_apply_size_hints(focused_client); /* TODO: return if no change */
        client_show(focused_client);
        xcb_flush(g_xcb);
    } else {
        if (focused_monitor->split < MAIN_SPLIT_MAX) {
            focused_monitor->split += MAIN_SPLIT_INC;
            monitor_render(focused_monitor);
            xcb_flush(g_xcb);
        }
    }
}

void focused_client_decrease_height()
{
    CHECK_FOCUSED

    if (focused_client->mode == MODE_FLOATING) {
        focused_client->f_height -= SIZE_INC;
        client_apply_size_hints(focused_client); /* TODO: return if no change */
        client_show(focused_client);
    } else {
        if (focused_monitor->split > MAIN_SPLIT_MIN) {
            focused_monitor->split -= MAIN_SPLIT_INC;
            monitor_render(focused_monitor);
        }
    }
    xcb_flush(g_xcb);
}

/* set this tag and this tag only */
void focused_client_set_tag(int tag)
{
    CHECK_FOCUSED

    /* do not change tagset of fullscreen client */
    if (IS_CLIENT_STATE(focused_client, STATE_FULLSCREEN))
        return;

    for (int i = 0; i < 32; ++i)
        if (focused_client->tagset & (1L << i))
            focused_monitor->tags[i]--;

    focused_client->tagset = (1L << (tag - 1));
    focused_client->tagset |= (1L << (tag - 1));
    focused_monitor->tags[tag - 1]++;

    monitor_render(focused_monitor);
    check_focus_consistency();
    hints_set_focused(focused_client);
    hints_set_monitor(focused_monitor);

    xcb_flush(g_xcb);
}

/* add or remove this tag */
void focused_client_toggle_tag(int tag)
{
    CHECK_FOCUSED
    /* do not change tagset of fullscreen client */
    if (IS_CLIENT_STATE(focused_client, STATE_FULLSCREEN))
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
        monitor_render(focused_monitor);

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
        if (stat(autostart, &st) == 0 && st.st_mode & S_IXUSR)
            system(autostart);
        else
            INFO("no mwmrc found.");
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
