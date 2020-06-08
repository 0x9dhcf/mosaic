#ifndef __MONITOR_H__
#define __MONITOR_H__

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
void monitor_update_main_views(Monitor *monitor, int by);
void monitor_render(Monitor *monitor, GeometryStatus status);

#endif
