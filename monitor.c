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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "mwm.h"
#include "log.h"
#include "monitor.h"
#include "client.h"
#include "hints.h"

#define DEFAULT_LAYOUT LT_RIGHT
#define DEFAULT_SPLIT .6
#define DEFAULT_MAINS 1

static void apply_none_layout(Monitor *m, int w_x, int w_y, int w_w, int w_h)
{
    Client *c;
    for (c = m->head; c; c = c->next)
        if (c->state == STATE_TILED && c->tagset & m->tagset) {
            c->t_x = w_x;
            c->t_y = w_y;
            c->t_width = w_w - 2 * c->border_width;
            c->t_height = w_h - 2 * c->border_width;
       }
}

static void horizontal_layout(
        Monitor *m,
        int mains, int stacked,
        int main_x, int main_y, int main_w, int main_h,
        int stack_x, int stack_y, int stack_w, int stack_h, int stack_r)
{
    int tiled = 0;

    /* tile the main views */
    Client *c;
    for (c = m->head; c && tiled < mains; c = c->next) {
        if (c->state == STATE_TILED && IS_VISIBLE(c)) {
            c->t_x = main_x;
            c->t_y = main_y;
            c->t_width = main_w - 2 * c->border_width;
            c->t_height = main_h - 2 * c->border_width;
            main_x += main_w;
            tiled++;
        }
    }

    /* tile the stack */
    tiled = 0;
    for (; c && tiled < stacked; c = c->next) {
        if (c->state == STATE_TILED && IS_VISIBLE(c)) {
            c->t_x = stack_x;
            c->t_y = stack_y;
            c->t_width = stack_w - 2 * c->border_width;
            c->t_height = (stack_h + stack_r) - 2 * c->border_width;
            stack_y += stack_h + stack_r;
            stack_r = 0;
            tiled++;
       }
    }
}

static void apply_right_layout(
        Monitor *m,
        int mains, int stacked,
        int w_x, int w_y, int w_w, int w_h)
{
    /* compute sizes */
    int main_w = stacked ? (w_w * m->split) / mains : (w_w / mains);
    int main_h = w_h;
    int stack_w = w_w - main_w * mains;
    int stack_h = stacked ? w_h / stacked : w_h;
    int stack_r = w_h - stacked * stack_h;

    /* compute positions */
    int main_x = w_x;
    int main_y = w_y;
    int stack_x = w_x + main_w * mains;
    int stack_y = w_y;

    horizontal_layout(
            m,
            mains, stacked,
            main_x, main_y, main_w, main_h,
            stack_x, stack_y, stack_w, stack_h, stack_r);
}

static void apply_left_layout(
        Monitor *m,
        int mains, int stacked,
        int w_x, int w_y, int w_w, int w_h)
{
    /* compute sizes */
    int main_w = stacked ? (w_w * m->split) / mains : (w_w / mains);
    int main_h = w_h;
    int stack_w = w_w - main_w * mains;
    int stack_h = stacked ? w_h / stacked : w_h;
    int stack_r = w_h - stacked * stack_h;

    /* compute positions */
    int main_x = w_x + stack_w;
    int main_y = w_y;
    int stack_x = w_x;
    int stack_y = w_y;

    horizontal_layout(
            m,
            mains, stacked,
            main_x, main_y, main_w, main_h,
            stack_x, stack_y, stack_w, stack_h, stack_r);
}

static void vertical_layout(
        Monitor *m,
        int mains, int stacked,
        int main_x, int main_y, int main_w, int main_h,
        int stack_x, int stack_y, int stack_w, int stack_h, int stack_r)
{
    int tiled = 0;

    /* tile the main views */
    Client *c;
    for (c = m->head; c && tiled < mains; c = c->next) {
        if (c->state == STATE_TILED && IS_VISIBLE(c)) {
            c->t_x = main_x;
            c->t_y = main_y;
            c->t_width = main_w - 2 * c->border_width;
            c->t_height = main_h - 2 * c->border_width;
            main_y += main_h;
            tiled++;
        }
    }

    /* tile the stack */
    tiled = 0;
    for (; c && tiled < stacked; c = c->next) {
        if (c->state == STATE_TILED && IS_VISIBLE(c)) {
            c->t_x = stack_x;
            c->t_y = stack_y;
            c->t_width = stack_w - 2 * c->border_width;
            c->t_height = (stack_h + stack_r) - 2 * c->border_width;
            stack_x += stack_w + stack_r;
            stack_r = 0;
            tiled++;
       }
    }
}

static void apply_bottom_layout(
        Monitor *m,
        int mains, int stacked,
        int w_x, int w_y, int w_w, int w_h)
{
    /* compute sizes */
    int main_w = w_w;
    int main_h = stacked ? (w_h * m->split) / mains : (w_h / mains);
    int stack_h = w_h - main_h * mains;
    int stack_w = stacked ? w_w / stacked : w_w;
    int stack_r = w_w - stacked * stack_w;

    /* compute positions */
    int main_x = w_x;
    int main_y = w_y;
    int stack_x = w_x;
    int stack_y = w_y + main_h * mains;

    vertical_layout(
            m,
            mains, stacked,
            main_x, main_y, main_w, main_h,
            stack_x, stack_y, stack_w, stack_h, stack_r);
}

static void apply_top_layout(
        Monitor *m,
        int mains, int stacked,
        int w_x, int w_y, int w_w, int w_h)
{
    /* compute sizes */
    int main_w = w_w;
    int main_h = stacked ? (w_h * m->split) / mains : (w_h / mains);
    int stack_h = w_h- main_h * mains;
    int stack_w = stacked ? w_w / stacked : w_w;
    int stack_r = w_w - stacked * stack_w;

    /* compute positions */
    int main_x = w_x;
    int main_y = w_y + stack_h;
    int stack_x = w_x;
    int stack_y = w_y;

    vertical_layout(
            m,
            mains, stacked,
            main_x, main_y, main_w, main_h,
            stack_x, stack_y, stack_w, stack_h, stack_r);
}

void monitor_initialize(Monitor *monitor, const char *name, int x, int y, int width, int height)
{
    strncpy(monitor->name, name, 127);
    monitor->x =  x;
    monitor->y = y;
    monitor->width = width;
    monitor->height = height;
    monitor->layout = DEFAULT_LAYOUT;
    monitor->split = DEFAULT_SPLIT;
    monitor->mains = DEFAULT_MAINS;
    memset(monitor->tags, 0, 32 * sizeof(int));
    monitor->tagset = 0x1;
    monitor->head = NULL;
    monitor->tail = NULL;
    monitor->next = NULL;
    monitor->prev = NULL;
}

void monitor_attach(Monitor *monitor, Client *client)
{
    client->monitor = monitor;
    if (HAS_PROPERTIES(client, PROPERTY_FOCUSABLE) ||
            HAS_PROPERTIES(client, PROPERTY_RESIZABLE)) {
        if (client->tagset < 0)
            client->tagset = monitor->tagset;
        for (int i = 0; i < 32; ++i)
            if (client->tagset & (1L << i))
                monitor->tags[i]++;
    }

    if (monitor->head) {
        client->next = monitor->head;
        monitor->head->prev = client;
    }

    if (! monitor->tail)
        monitor->tail = client;

    monitor->head = client;
    client->prev = NULL;

    /* default policy for floatings other than fixed
     * is to be centered on the monitor */
    if (HAS_PROPERTIES(client, PROPERTY_RESIZABLE)) {
        client->f_x = monitor->x + (monitor->width - client->f_width) / 2;
        client->f_y = monitor->y + (monitor->height - client->f_height) / 2;
    }
}

void monitor_detach(Monitor *monitor, Client *client)
{
    if (client->monitor != monitor)
        return;

    /* remove the client from this monitor */
    if (client->prev)
        client->prev->next = client->next;
    else
        monitor->head = client->next;

    if (client->next)
        client->next->prev = client->prev;
    else
        monitor->tail = client->prev;

    for (int i = 0; i < 32; ++i)
        if (client->tagset & (1L << i))
            monitor->tags[i]--;

    client->monitor = NULL;
    client->next = NULL;
    client->monitor = NULL;
}

void monitor_render(Monitor *monitor)
{
    int tilables = 0;
    int fullscreen = 0;
    int rr = 0, rl = 0, rt = 0, rb = 0, wx = 0, wy = 0, ww = 0, wh = 0;

    /* first round to get information about clients */
    for (Client *c = monitor->head; c; c = c->next) {
        if (c->state == STATE_FULLSCREEN)
            fullscreen++;

        /* do not hide transient nor fixed */
        if (! c->transient && ! IS_FIXED(c))
            client_hide(c);

        if (c->state == STATE_TILED && IS_VISIBLE(c))
            tilables++;

        /* XXX: the client position should be considered
         * especially if there's several client reserving space */
        if (IS_VISIBLE(c)) {
            rl = MAX(rl, c->reserved_left);
            rr = MAX(rr, c->reserved_right);
            rt = MAX(rt, c->reserved_top);
            rb = MAX(rb, c->reserved_bottom);
        }
        wx = rl;
        wy = rt;
        ww = monitor->width - (rl + rr);
        wh = monitor->height - (rt + rb);
    }

    int mains = tilables < monitor->mains ? tilables : monitor->mains;
    int stacked = (tilables - mains) > 0 ? (tilables - mains ) : 0;
    /* compute tiles positions */
    if (tilables && !fullscreen) {
        switch(monitor->layout) {
            case LT_RIGHT:
                apply_right_layout(monitor, mains, stacked, wx, wy, ww, wh);
                break;
            case LT_LEFT:
                apply_left_layout(monitor, mains, stacked, wx, wy, ww, wh);
                break;
            case LT_BOTTOM:
                apply_bottom_layout(monitor, mains, stacked, wx, wy, ww, wh);
                break;
            case LT_TOP:
                apply_top_layout(monitor, mains, stacked, wx, wy, ww, wh);
                break;
            default:
                apply_none_layout(monitor, wx, wy, ww, wh);
                break;
        }
    }

    /* display clients */
    for (Client *c = monitor->head; c; c = c->next) {
        if (fullscreen && c->state != STATE_FULLSCREEN)
            continue;
        if (! IS_FIXED(c))
            client_show(c);
    }
}

int monitor_increase_main_views(Monitor *monitor)
{
    int tilables = 0;
    for (Client *c = monitor->head; c; c = c->next)
        if (c->state == STATE_TILED && IS_VISIBLE(c))
                tilables++;

    if (tilables > monitor->mains) {
        monitor->mains++;
        return 1;
    }
    return 0;
}

int monitor_decrease_main_views(Monitor *monitor)
{
    if (monitor->mains > 1) {
        monitor->mains--;
        return 1;
    }
    return 0;
}

