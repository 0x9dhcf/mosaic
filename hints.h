#ifndef __HINTS_H__
#define __HINTS_H__

typedef struct _Monitor Monitor;
typedef struct _Client Client;

void hints_set_monitor(Monitor *monitor);
void hints_set_focused(Client *client);
void hints_update_focused_color();

#endif
