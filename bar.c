/*
 * Copyright (c) 2019-2020 Pierre Evenou
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

#include <xcb/xcb.h>

#include "bar.h"
#include "log.h"
#include "monitor.h"
#include "mosaic.h"
#include "rectangle.h"
#include "settings.h"
#include "x11.h"

#define TAGS 10

Bar g_bar;

void bar_open(Monitor *monitor)
{
    g_bar.opened = 0;
    g_bar.monitor = monitor;
    g_bar.window = xcb_generate_id(g_xcb);
    xcb_create_window(
            g_xcb,
            XCB_COPY_FROM_PARENT,
            g_bar.window,
            g_root,
            0, 0, 1, 1,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            XCB_CW_BACK_PIXEL |
            XCB_CW_BORDER_PIXEL |
            XCB_CW_OVERRIDE_REDIRECT |
            XCB_CW_EVENT_MASK,
            (int[]) {
                g_bgcolor,
                g_fgcolor,
                0,
                XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS });

    g_bar.font = xcb_generate_id(g_xcb);
    xcb_open_font(
            g_xcb,
            g_bar.font,
            strlen(g_font),
            g_font);

    g_bar.gcontext = xcb_generate_id(g_xcb);
    xcb_create_gc(
            g_xcb,
            g_bar.gcontext,
            g_bar.window,
            XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT,
            (int []) {
                g_fgcolor,
                g_bgcolor,
                g_bar.font
            });
}

void bar_show()
{
    xcb_configure_window(
            g_xcb,
            g_bar.window,
            XCB_CONFIG_WINDOW_X |
            XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT,
            (int[]) {
                g_bar.monitor->geometry.x,
                g_bar.monitor->geometry.y,
                g_bar.monitor->geometry.width,
                BAR_HEIGHT,
            });

    xcb_map_window(g_xcb, g_bar.window);

    g_bar.opened = 1;
}

void bar_hide()
{
    g_bar.opened = 0;
    xcb_unmap_window(g_xcb, g_bar.window);
}

void bar_display(int mtags[32], int mtagset, char *cname, int ctagset)
{
    if (! g_bar.opened)
        return;

    /* clear the window */
    xcb_clear_area(
            g_xcb,
            0,
            g_bar.window,
            0, 0,
            g_bar.monitor->geometry.width, BAR_HEIGHT);

    /* focused monitor tags */
    int pos = 0;
    for (int i = 0; i < 32; ++i) {
        if (mtags[i] || mtagset & (1L << i) ) {
            /* fgcolor tag are active, grey ones are inactive */
            xcb_change_gc(
                    g_xcb,
                    g_bar.gcontext,
                    XCB_GC_FOREGROUND,
                    (const int []) {
                        mtagset & (1L << (i)) ?
                            g_fgcolor :
                            0x666666 });

            char str[2];
            sprintf(str, "%d", i+1);
            xcb_image_text_8(
                    g_xcb,
                    strlen(str),
                    g_bar.window,
                    g_bar.gcontext,
                    TAGS + (20 * pos++) , 16,
                    str);
        }
    }

    xcb_change_gc(
        g_xcb,
        g_bar.gcontext,
        XCB_GC_FOREGROUND,
        (const int []) { g_fgcolor }); 
    
    if (cname) {
        xcb_image_text_8(
                g_xcb,
                strlen(cname),
                g_bar.window,
                g_bar.gcontext,
                .20 * g_bar.monitor->geometry.width, 16,
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
                    g_bar.window,
                    g_bar.gcontext,
                    .40 * g_bar.monitor->geometry.width + (20 * pos++) , 16,
                    str);
        }
    }
/*
    xcb_query_text_extents_reply_t *extents = xcb_query_text_extents_reply(
            g_xcb,
            xcb_query_text_extents (
                    g_xcb,
                    g_bar.font,
                    strlen(VERSION),
                    VERSION),
            NULL);
*/
    /* version */
    xcb_image_text_8(
            g_xcb,
            strlen(VERSION),
            g_bar.window,
            g_bar.gcontext,
            .97 * g_bar.monitor->geometry.width, 16,
            VERSION);
}

void bar_close()
{
    xcb_destroy_window(g_xcb, g_bar.window);
    xcb_close_font(g_xcb, g_bar.font);
}
