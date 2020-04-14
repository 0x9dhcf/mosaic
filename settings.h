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

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xkb.h>
#include <xcb/randr.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-x11.h>

#define AUTOSTART ".mwmrc"

#ifndef NDEBUG
#define MODKEY XCB_MOD_MASK_1
#else
#define MODKEY XCB_MOD_MASK_4
#endif

typedef enum  _CallbackType {
    CB_VOID,
    CB_INT
} CallbackType ;

typedef struct _Shortcut {
    struct {
        unsigned int modifier;
        xkb_keysym_t keysym;
    } sequence;
    CallbackType type;
    union {
        void (*vcb)();
        void (*icb)();
    } callback;
    union { int i; } arg;
} Shortcut;

typedef struct _Rule {
    char *class_name;
    char *instance_name;
    int tags;
    int mode; /* see client mode */
} Rule;

/* static configuration */
extern unsigned int     g_border_width;
extern unsigned int     g_normal_color;
extern unsigned int     g_focused_color;
extern unsigned int     g_urgent_color;
extern unsigned int     g_hud_bgcolor;
extern char             g_hud_font_face[256];
extern unsigned int     g_hud_font_size;
extern Rule             g_rules[];
extern Shortcut         g_shortcuts[]; 


