#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "bar.h"
#include "hints.h"
#include "log.h"
#include "monitor.h"
#include "mosaic.h"
#include "settings.h"
#include "client.h"

#define DEFAULT_LAYOUT LT_RIGHT
#define DEFAULT_MAINS 1

static void apply_none_layout(
        Monitor *m,
        int w_x,
        int w_y,
        int w_w,
        int w_h);
static void horizontal_layout(
        Monitor *m,
        int mains, int stacked,
        int main_x, int main_y, int main_w, int main_h,
        int stack_x, int stack_y, int stack_w, int stack_h, int stack_r);
static void apply_right_layout(
        Monitor *m,
        int mains, int stacked,
        int w_x, int w_y, int w_w, int w_h);
static void apply_left_layout(
        Monitor *m,
        int mains, int stacked,
        int w_x, int w_y, int w_w, int w_h);
static void vertical_layout(
        Monitor *m,
        int mains, int stacked,
        int main_x, int main_y, int main_w, int main_h,
        int stack_x, int stack_y, int stack_w, int stack_h, int stack_r);
static void apply_bottom_layout(
        Monitor *m,
        int mains, int stacked,
        int w_x, int w_y, int w_w, int w_h);
static void apply_top_layout(
        Monitor *m,
        int mains, int stacked,
        int w_x, int w_y, int w_w, int w_h);

void
apply_none_layout(Monitor *m, int w_x, int w_y, int w_w, int w_h)
{
    Client *c;
    for (c = m->head; c; c = c->next)
        if (c->mode == MODE_TILED && c->tagset & m->tagset)
            client_set_tiling(c, &(Rectangle) { w_x, w_y, w_w, w_h });
}

void
horizontal_layout(
        Monitor *m,
        int mains, int stacked,
        int main_x, int main_y, int main_w, int main_h,
        int stack_x, int stack_y, int stack_w, int stack_h, int stack_r)
{
    int tiled = 0;

    /* tile the main views */
    Client *c;
    for (c = m->head; c && tiled < mains; c = c->next) {
        if (c->mode == MODE_TILED && client_is_visible(c)) {
            client_set_tiling(c, &(Rectangle) {main_x, main_y, main_w, main_h});
            main_x += main_w;
            tiled++;
        }
    }

    /* tile the stack */
    tiled = 0;
    for (; c && tiled < stacked; c = c->next) {
        if (c->mode == MODE_TILED && client_is_visible(c)) {
            client_set_tiling(c, &(Rectangle) {stack_x, stack_y, stack_w, (stack_h + stack_r)});
            stack_y += stack_h + stack_r;
            stack_r = 0;
            tiled++;
       }
    }
}

void
apply_right_layout(
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

void
apply_left_layout(
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

void
vertical_layout(
        Monitor *m,
        int mains, int stacked,
        int main_x, int main_y, int main_w, int main_h,
        int stack_x, int stack_y, int stack_w, int stack_h, int stack_r)
{
    int tiled = 0;

    /* tile the main views */
    Client *c;
    for (c = m->head; c && tiled < mains; c = c->next) {
        if (c->mode == MODE_TILED && client_is_visible(c)) {
            client_set_tiling(c, &(Rectangle) {main_x, main_y, main_w, main_h});
            main_y += main_h;
            tiled++;
        }
    }

    /* tile the stack */
    tiled = 0;
    for (; c && tiled < stacked; c = c->next) {
        if (c->mode == MODE_TILED && client_is_visible(c)) {
            client_set_tiling(c, &(Rectangle) {stack_x, stack_y, stack_w, (stack_h + stack_r)});
            stack_x += stack_w + stack_r;
            stack_r = 0;
            tiled++;
       }
    }
}

void
apply_bottom_layout(
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

void
apply_top_layout(
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

void
monitor_initialize(Monitor *monitor, const char *name, int x, int y, int width, int height)
{
    strncpy(monitor->name, name, 127);
    monitor->geometry = (Rectangle) { x, y, width, height };
    monitor->layout = DEFAULT_LAYOUT;
    monitor->split = g_split;
    monitor->mains = DEFAULT_MAINS;
    memset(monitor->tags, 0, 32 * sizeof(int));
    monitor->tagset = 1;
    monitor->head = NULL;
    monitor->tail = NULL;
    monitor->next = NULL;
    monitor->prev = NULL;
}

void
monitor_attach(Monitor *monitor, Client *client)
{
    client->monitor = monitor;
    if (client->tagset < 0)
        client->tagset = monitor->tagset;
    for (int i = 0; i < 32; ++i)
        if (client->tagset & (1L << i))
            monitor->tags[i]++;

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
    client->floating_geometry.x =
        (monitor->geometry.x + monitor->geometry.width / 2) -
        client->floating_geometry.width / 2;
    client->floating_geometry.y =
        (monitor->geometry.y + monitor->geometry.height / 2) -
        client->floating_geometry.height / 2;
}

void
monitor_detach(Monitor *monitor, Client *client)
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
}

void
monitor_update_main_views(Monitor *monitor, int by)
{
    if (by > 0) {

        int tilables = 0;
        for (Client *c = monitor->head; c; c = c->next)
            if (c->mode == MODE_TILED && client_is_visible(c))
                tilables++;

        if (tilables >= monitor->mains + by)
            monitor->mains += by;
        else
            monitor->mains = tilables;
    } else {
        if (monitor->mains + by > 1)
            monitor->mains += by;
        else
            monitor->mains = 1;
    }
}

void
monitor_render(Monitor *monitor, GeometryStatus status)
{
    int tilables = 0;
    int fullscreen = 0;
    int rr = 0, rl = 0, rt = 0, rb = 0, wx = 0, wy = 0, ww = 0, wh = 0;

    if (bar_is_monitor(monitor) && bar_is_opened())
        rt = g_bar_height;

    /* first round to get information about clients. */
    for (Client *c = monitor->head; c; c = c->next) {
        if (c->mode == MODE_FULLSCREEN)
            fullscreen++;

        if ((c->state & STATE_STICKY) != STATE_STICKY || ! status)
            client_hide(c);

        if (c->mode == MODE_TILED && client_is_visible(c))
            tilables++;

        /* XXX: the client position should be considered
         * especially if there's several client reserving space */
        if (client_is_visible(c)) {
            rl = MAX(rl, c->strut.left);
            rr = MAX(rr, c->strut.right);
            rt = MAX(rt, c->strut.top);
            rb = MAX(rb, c->strut.bottom);
        }
    }
    wx = monitor->geometry.x + rl;
    wy = monitor->geometry.y + rt;
    ww = monitor->geometry.width - (rl + rr);
    wh = monitor->geometry.height - (rt + rb);

    int mains = tilables < monitor->mains ? tilables : monitor->mains;
    int stacked = (tilables - mains) > 0 ? (tilables - mains ) : 0;

#if 0
#ifndef NDEBUG
    DEBUG("rendering: %s - (%d, %d), [%d x %d]", monitor->name, wx, wy, ww, wh);
    DEBUG("\ttilables: %d", tilables);
    DEBUG("\tmains: %d", mains);
    DEBUG("\tstacked: %d", stacked);
    DEBUG("\tfullscreen: %d", fullscreen);
    DEBUG("\thead: %p", monitor->head);
    DEBUG("\ttail: %p", monitor->tail);
    for (Client *c = monitor->head; c; c = c->next)
        DEBUG("\tclient: %p", c);
#endif
#endif
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
        /* if we have some fullscreen, display only those */
        if (fullscreen && (c->mode != MODE_FULLSCREEN))
            continue;

        if (client_is_visible(c) && ! c->transient &&
                (((c->state & STATE_STICKY) != STATE_STICKY) || ! status))
            client_show(c);
    }


    /* last round for the transients for.
     * only now we know where they belong. */
    for (Client *c = monitor->head; c; c = c->next) {
        if (c->transient) {
            Client *t = lookup(c->transient);
            if (t) {
                Rectangle r = t->mode == MODE_TILED ?
                    t->tiling_geometry : t->floating_geometry;
                c->floating_geometry.x = (r.x + r.width / 2) -
                    c->floating_geometry.width / 2;
                c->floating_geometry.y = (r.y + r.height / 2) -
                    c->floating_geometry.height / 2;
                client_show(c);
            }
        }
    }
}

