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

#ifndef __MWM_BAR_H__
#define __MWM_BAR_H__

#include <xcb/xcb.h>

#include "rectangle.h"

#define BAR_HEIGHT 24

typedef struct _Monitor Monitor;

typedef struct _Bar {
    xcb_window_t    window;
    xcb_font_t      font;
    xcb_gcontext_t  gcontext;
    int             opened;
    Monitor         *monitor;
} Bar;

extern Bar g_bar;

void bar_open(Monitor *monitor);
void bar_show();
void bar_hide();
void bar_display(int mtags[32], int mtagset, char *cname, int ctagset);
void bar_close();

#endif
