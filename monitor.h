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

#ifndef __MONITOR_H__
#define __MONITOR_H__

#include <xcb/xcb.h>

#include "rectangle.h"

typedef struct _Client Client;
typedef struct _Bar Bar;

typedef enum _Layout {
    LT_NONE,
    LT_TOP,
    LT_LEFT,
    LT_BOTTOM,
    LT_RIGHT
} Layout;

typedef enum _GeometryStatus {
    GS_CHANGED,
    GS_UNCHANGED
} GeometryStatus;

typedef struct _Monitor {
    char                name[128];
    Rectangle           geometry;
    Layout              layout;
    float               split;
    int                 mains;
    int                 tags[32];
    int                 tagset;
    Client              *head;
    Client              *tail;
    struct _Monitor     *next;
    struct _Monitor     *prev;
} Monitor;

void monitor_initialize(Monitor *monitor, const char *name, int x, int y, int width, int height);
void monitor_attach(Monitor *monitor, Client *client);
void monitor_detach(Monitor *monitor, Client *client);
void monitor_render(Monitor *monitor, GeometryStatus status);
void monitor_update_main_views(Monitor *monitor, int by);

#endif
