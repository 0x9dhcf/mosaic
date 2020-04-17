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

#include "settings.h"
#include "mwm.h"

#define SIZE_INC    30

#define K_M         MODKEY
#define K_MC        MODKEY | XCB_MOD_MASK_CONTROL
#define K_MS        MODKEY | XCB_MOD_MASK_SHIFT
#define K_MCS       MODKEY | XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_SHIFT

/* static "extern" configuration */
unsigned int    g_border_width      = 1;
unsigned int    g_normal_color      = 0x888888;
unsigned int    g_focused_color     = 0x0000ff;
unsigned int    g_urgent_color      = 0xff0000;

Shortcut g_shortcuts[] = {
    /* global */
    {{K_MS,     XKB_KEY_Home},      CB_VOID,    {quit},                                 {}},
    {{K_MS,     XKB_KEY_d},         CB_VOID,    {dump},                                 {}},
    {{K_MS,     XKB_KEY_b},         CB_VOID,    {toggle_bar},                           {}},

    /* focus */
    {{K_M,      XKB_KEY_Left},      CB_VOID,    {focus_previous_client},                {}},
    {{K_M,      XKB_KEY_Up},        CB_VOID,    {focus_previous_client},                {}},
    {{K_M,      XKB_KEY_Right},     CB_VOID,    {focus_next_client},                    {}},
    {{K_M,      XKB_KEY_Down},      CB_VOID,    {focus_next_client},                    {}},
    {{K_M,      XKB_KEY_period},    CB_VOID,    {focus_next_monitor},                   {}},
    {{K_M,      XKB_KEY_comma},     CB_VOID,    {focus_previous_monitor},               {}},

    /* monitors */
    {{K_MC,     XKB_KEY_Page_Up},   CB_INT,     {focused_monitor_update_main_views},    {1}},
    {{K_MC,     XKB_KEY_Page_Down}, CB_INT,     {focused_monitor_update_main_views},    {-1}},
    {{K_MC,     XKB_KEY_space},     CB_INT,     {focused_monitor_set_layout},           {LT_NONE}},
    {{K_MC,     XKB_KEY_Right},     CB_INT,     {focused_monitor_set_layout},           {LT_RIGHT}},
    {{K_MC,     XKB_KEY_Left},      CB_INT,     {focused_monitor_set_layout},           {LT_LEFT}},
    {{K_MC,     XKB_KEY_Down},      CB_INT,     {focused_monitor_set_layout},           {LT_BOTTOM}},
    {{K_MC,     XKB_KEY_Up},        CB_INT,     {focused_monitor_set_layout},           {LT_TOP}},
    {{K_M,      XKB_KEY_Tab},       CB_VOID,    {focused_monitor_rotate_clockwise},     {}},
    {{K_MS,     XKB_KEY_Tab},       CB_VOID,    {focused_monitor_rotate_counter_clockwise}, {}},
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
    {{K_MS,     XKB_KEY_Up},        CB_INT,     {focused_client_move},                  {D_UP}},
    {{K_MS,     XKB_KEY_Down},      CB_INT,     {focused_client_move},                  {D_DOWN}},
    {{K_MS,     XKB_KEY_Left},      CB_INT,     {focused_client_move},                  {D_LEFT}},
    {{K_MS,     XKB_KEY_Right},     CB_INT,     {focused_client_move},                  {D_RIGHT}},
    {{K_MS,     XKB_KEY_period},    CB_VOID,    {focused_client_to_next_monitor},       {}},
    {{K_MS,     XKB_KEY_comma },    CB_VOID,    {focused_client_to_previous_monitor},   {}},
    {{K_MCS,    XKB_KEY_Up},        CB_INT_INT, {focused_client_resize},                {0, -30}},
    {{K_MCS,    XKB_KEY_Down},      CB_INT_INT, {focused_client_resize},                {0, 30}},
    {{K_MCS,    XKB_KEY_Left},      CB_INT_INT, {focused_client_resize},                {-30, 0}},
    {{K_MCS,    XKB_KEY_Right},     CB_INT_INT, {focused_client_resize},                {30, 0}},
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
