#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xkb.h>
#include <xcb/randr.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-x11.h>

#define AUTOSTART ".mosaicrc"

#ifndef NDEBUG
#define MODKEY XCB_MOD_MASK_1
#else
#define MODKEY XCB_MOD_MASK_4
#endif

typedef enum  _CallbackType {
    CB_VOID,
    CB_INT,
    CB_INT_INT
} CallbackType ;

typedef struct _KeySequence {
    unsigned int modifier;
    xkb_keysym_t keysym;
} KeySequence;

typedef struct _Shortcut {
    KeySequence     sequence;
    CallbackType    type;
    union {
        void (*vcb)();
        void (*icb)(int);
        void (*iicb)(int, int);
    } callback;
    int args[2];
} Shortcut;

typedef struct _Binding {
    KeySequence     sequence;
    char            *args[16];
} Binding;

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
extern unsigned int     g_bgcolor;
extern unsigned int     g_fgcolor;
extern unsigned int     g_bar_bgcolor;
extern unsigned int     g_bar_fgcolor;
extern unsigned int     g_bar_selected_tag_fgcolor;
extern unsigned int     g_bar_selected_tag_bgcolor;
extern double           g_split;
extern char             g_font[256];
extern unsigned int     g_bar_height;
extern Rule             g_rules[];
extern Shortcut         g_shortcuts[]; 
extern Binding          g_bindings[]; 

#endif
