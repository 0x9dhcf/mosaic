#include <stdbool.h>
#include <string.h>

#include "log.h"
#include "monitor.h"
#include "mosaic.h"
#include "rectangle.h"
#include "settings.h"
#include "bar.h"

#define PADDING 10
#define WINDOW_MASK XCB_CW_BACK_PIXEL | \
                    XCB_CW_BORDER_PIXEL |\
                    XCB_CW_OVERRIDE_REDIRECT |\
                    XCB_CW_EVENT_MASK |\
                    XCB_CW_COLORMAP

static char *text(char *s);
static char *extract(char *c, char t);
static void display_string(char *s, int x, int y);
static void display_char(char c, int *x, int *y);
static int change_color(char *c);
static void clear(Rectangle *area);

static xcb_window_t     window;
static xcb_pixmap_t     pixmap;
static xcb_gcontext_t   gcontext;
static bool             opened;
static Monitor          *monitor;
static xcb_font_t       font;
static Rectangle        left;
static Rectangle        center;
static Rectangle        right;

/*
 * remove formating tags from the given string
 * the retuned string should be freed.
 */
char*
text(char *s)
{
    char *p, *t, *u;
    int len = 0;

    p = s;
    while (*p != '\0' && *p != '\n') {
        if (p[0] == '%' && p[2] == '{')
            while (*p++ != '}');
        p++;
        len++;
    }

    t = malloc(len+1);
    p = s; u = t;
    while (*p != '\0' && *p != '\n') {
        if (p[0] == '%' && p[2] == '{')
            while (*p++ != '}');
        *u++ = *p++;
    }
    *u = '\0';

    return t;
}

/*
 * extract the tag %t{ from the string c
 * the retuned string should be freed.
 */
char *
extract(char *c, char t)
{
    char *s, *p, *q;
    int len = 0;
    bool in = false;
    int level = 0;

    p = c;
    while (*p != '\0' && *p != '\n') {
        if (p[0] == '%' && p[1] == t && p[2] == '{') { in = true; level++; p += 3; }
        if (*p == '{') { level++; }
        if (*p == '}') { if (!--level) in = false; }
        if (in) len++;
        p++;
    }

    if (!len)
        return NULL;

    s = malloc(len + 1);

    p = c;
    q = s;
    while (*p != '\0' && *p != '\n') {
        if (p[0] == '%' && p[1] == t && p[2] == '{') { in = true; level++; p += 3; }
        if (*p == '{') { level++; }
        if (*p == '}') { if (!--level) in = false; }
        if (in) *q++ = *p;
        p++;
    }
    *q = '\0';

    return s;
}

void
display_string(char *s, int x, int y)
{
    while (*s != '\0' && *s != '\n') {
        if (s[0] == '%' && s[2] == '{') {
            switch (s[1]) {
                case 'f' : s += change_color(s) + 1; /* eat the color and move to the next char */
                break;
                // TODO vskip, hskip?
            }
        }
        if (*s != '\0') /* happens when the string ends by a tag */
            display_char(*s++, &x, &y);
    }
};

void
display_char(char c, int *x, int *y)
{
    xcb_image_text_8(
           g_xcb,
           1,
           pixmap,
           gcontext,
           *x, *y,
           &c);

    xcb_query_text_extents_reply_t *e;
    e = xcb_query_text_extents_reply(
            g_xcb,
            xcb_query_text_extents (
                    g_xcb,
                    font,
                    1,
                    (xcb_char2b_t*)&c),
            NULL);

    if (e) {
        *x += e->overall_width;
        free(e);
    }
}

/* change foregroung color
 * return the number of chars read */
int
change_color(char *c)
{
    char col[7] = {'\0'};
    char *p = c;
    int len = 0;

    p += 3;
    while (*p != '}') { col[len++] = *p++; }
    if (len != 6)
        return len;

    xcb_change_gc(
            g_xcb,
            gcontext,
            XCB_GC_FOREGROUND,
            (const unsigned int []) {
                (unsigned int)strtoul(col, NULL, 16)
            });
    return len + 3;
}

void
clear(Rectangle *area)
{
    xcb_change_gc(
            g_xcb,
            gcontext,
            XCB_GC_FOREGROUND,
            (const int []) { g_bar_bgcolor });

    xcb_poly_fill_rectangle(
            g_xcb,
            pixmap,
            gcontext,
            1,
            (const xcb_rectangle_t []) { {
                area->x,
                area->y,
                area->width,
                area->height } });

    xcb_change_gc(
            g_xcb,
            gcontext,
            XCB_GC_FOREGROUND | XCB_GC_BACKGROUND,
            (const int []) { g_bar_fgcolor , g_bar_bgcolor });

}

void
bar_open(Monitor *m)
{
    int x, y, depth;
    unsigned int w, h;

    opened = false;
    monitor = m;

    depth = g_screen->root_depth;
    window = xcb_generate_id(g_xcb);
    xcb_create_window(
            g_xcb,
            depth,
            window,
            g_root,
            m->geometry.x,
            m->geometry.y,
            m->geometry.width,
            g_bar_height,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            g_visual->visual_id,
            WINDOW_MASK,
            (int[]) {
                g_bar_bgcolor,
                g_bar_fgcolor,
                0,
                XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS,
                g_colormap });

    x = m->geometry.x;
    y = m->geometry.y;
    w = m->geometry.width / 3;
    h = g_bar_height;

    left = (Rectangle) {x, y, w, h};
    right = (Rectangle) {m->geometry.width - w, y, w, h};
    center = (Rectangle) {x + w, y, m->geometry.width - 2 * w, h};

    pixmap = xcb_generate_id(g_xcb);
    xcb_create_pixmap(
            g_xcb,
            depth,
            pixmap,
            window,
            m->geometry.width,
            g_bar_height);

    font = xcb_generate_id(g_xcb);
    xcb_open_font(
           g_xcb,
           font,
           strlen(g_font),
           g_font);

    gcontext = xcb_generate_id(g_xcb);
    xcb_create_gc(
           g_xcb,
           gcontext,
           pixmap,
           XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT,
           (int []) {
               g_bar_fgcolor,
               g_bar_bgcolor,
               font
           });
}

void
bar_show()
{
    xcb_map_window(g_xcb, window);
    opened = true;
}

bool
bar_is_opened()
{
    return opened;
}

bool
bar_is_monitor(Monitor *m)
{
    return monitor == m;
}

bool
bar_is_window(xcb_window_t w)
{
    return window == w;
}

void
bar_hide()
{
    opened = 0;
    xcb_unmap_window(g_xcb, window);
}

void
bar_display_wmstatus(int mtags[32], int mtagset, char *cname, int ctagset)
{
    if (! opened)
        return;

    int pty, ptw, pcy;
    pty = ptw = pcy = 0;

    /* tags will be numbers only compute the vertical position thanks to a fake string */
    xcb_query_text_extents_reply_t *e = xcb_query_text_extents_reply(
            g_xcb,
            xcb_query_text_extents (
                    g_xcb,
                    font,
                    strlen("0123456789"),
                    (xcb_char2b_t*)"0123456789"),
            NULL);

    if (e) {
        pty = g_bar_height - (g_bar_height - (e->overall_ascent + e->overall_descent)) / 2;
        ptw = e->overall_width;
        free(e);
    }

    /* clear the pixmap */
    clear(&left);

    /* focused monitor tags */
    int pos = 0;
    for (int i = 0; i < 32; ++i) {
        if (mtags[i] || mtagset & (1L << i) ) {
            int x = left.x + PADDING + (24 * pos++);
            char str[2];
            sprintf(str, "%d", i+1);

            xcb_query_text_extents_reply_t *e = xcb_query_text_extents_reply(
                    g_xcb,
                    xcb_query_text_extents (
                            g_xcb,
                            font,
                            strlen(str),
                            (xcb_char2b_t*)str),
                    NULL);

            if (e) {
                pty = g_bar_height - (g_bar_height - (e->overall_ascent + e->overall_descent)) / 2;
                ptw = e->overall_width;
                free(e);
            }

            xcb_change_gc(
                g_xcb,
                gcontext,
                XCB_GC_FOREGROUND | XCB_GC_BACKGROUND,
                (const int []) {
                    mtagset & (1L << (i)) ?  g_bar_selected_tag_bgcolor : g_bar_bgcolor,
                    mtagset & (1L << (i)) ?  g_bar_selected_tag_fgcolor : g_bar_fgcolor});

            xcb_rectangle_t r = (xcb_rectangle_t) {x - 12, 0, 24, g_bar_height};
            xcb_poly_fill_rectangle(g_xcb, pixmap, gcontext, 1, &r);

            xcb_change_gc(
                g_xcb,
                gcontext,
                XCB_GC_FOREGROUND | XCB_GC_BACKGROUND,
                (const int []) {
                    mtagset & (1L << (i)) ?  g_bar_selected_tag_fgcolor : g_bar_fgcolor,
                    mtagset & (1L << (i)) ?  g_bar_selected_tag_bgcolor : g_bar_bgcolor});

            xcb_image_text_8(
                    g_xcb,
                    strlen(str),
                    pixmap,
                    gcontext,
                    x - ptw / 2 , pty,
                    str);
            }
    }

    /* focused client name */
    xcb_change_gc(
        g_xcb,
        gcontext,
        XCB_GC_FOREGROUND | XCB_GC_BACKGROUND,
        (const int []) { g_bar_fgcolor, g_bar_bgcolor });

    e = xcb_query_text_extents_reply(
            g_xcb,
            xcb_query_text_extents (
                    g_xcb,
                    font,
                    strlen(cname),
                    (xcb_char2b_t*)cname),
            NULL);

    if (e) {
        pcy = g_bar_height - (g_bar_height - (e->overall_ascent + e->overall_descent)) / 2;
        free(e);
    }

    if (cname) {
        xcb_image_text_8(
                g_xcb,
                strlen(cname),
                pixmap,
                gcontext,
                left.x + 250, pcy,
                cname);
    }

    pos = 0;
    for (int i = 0; i < (int)sizeof(int) * 8; ++i) {
        if (ctagset & (1L << (i))) {
            char str[2];
            sprintf(str, "%d", i+1);
            xcb_image_text_8(
                    g_xcb,
                    strlen(str),
                    pixmap,
                    gcontext,
                    left.x + 350 + (20 * pos++) , pty,
                    str);
        }
    }

    xcb_copy_area(
            g_xcb,
            pixmap,
            window,
            gcontext,
            left.x, left.y,
            left.x, left.y,
            left.width, left.height);
}

void
bar_display_systatus()
{
    char status[4096];

    xcb_icccm_get_text_property_reply_t name;
    xcb_icccm_get_wm_name_reply(
            g_xcb,
            xcb_icccm_get_wm_name(g_xcb, g_root),
            &name,
            NULL);
    if (name.name_len) {
        snprintf(status, name.name_len + 1, "%s", name.name);
        xcb_icccm_get_text_property_reply_wipe(&name);
    } else {
        strcpy(status, "%c{%f{ff0000}Error}");
    }

    char *ct = NULL;
    char *rt = NULL;

    /* find center tags */
    ct = extract(status, 'c');
    if (ct) {
        char *txt = 0;
        int posx, posy;

        posx = center.x;
        posy = center.y;

        txt = text(ct);
        if (! txt)
            return;

        xcb_query_text_extents_reply_t *e = xcb_query_text_extents_reply(
                g_xcb,
                xcb_query_text_extents (
                        g_xcb,
                        font,
                        strlen(txt),
                        (xcb_char2b_t*)txt),
                NULL);

        if (e) {
            posx += (center.width - e->overall_width) / 2;
            posy += g_bar_height - (g_bar_height - (e->overall_ascent + e->overall_descent)) / 2;
            free(e);
        }

        clear(&center);

        display_string(ct, posx, posy);

        free(txt);
        free(ct);

        xcb_copy_area(
                g_xcb,
                pixmap,
                window,
                gcontext,
                center.x, center.y,
                center.x, center.y,
                center.width, center.height);
    }

    /* find right tags */
    rt = extract(status, 'r');
    if (rt) {
        char *txt = 0;
        int posx, posy;
        posx = right.x;
        posy = right.y;

        txt = text(rt);
        if (! txt)
            return;

        xcb_query_text_extents_reply_t *e = xcb_query_text_extents_reply(
                g_xcb,
                xcb_query_text_extents (
                        g_xcb,
                        font,
                        strlen(txt),
                        (xcb_char2b_t*)txt),
                NULL);

        if (e) {
            posx += right.width - (e->overall_width + PADDING);
            posy += g_bar_height - (g_bar_height - (e->overall_ascent + e->overall_descent)) / 2;
            free(e);
        }

        clear(&right);

        display_string(rt, posx, posy);

        free(txt);
        free(rt);

        xcb_copy_area(
                g_xcb,
                pixmap,
                window,
                gcontext,
                right.x, right.y,
                right.x, right.y,
                right.width, right.height);
    }

}

void
bar_close()
{
    if (opened) {
        xcb_free_pixmap(g_xcb, pixmap);
        xcb_destroy_window(g_xcb, window);
        xcb_close_font(g_xcb, font);
    }
}
