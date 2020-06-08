#ifndef __BAR_H__
#define __BAR_H__

#include <stdbool.h>

#include "x11.h"

typedef struct _Monitor Monitor;

void bar_open(Monitor *m);
bool bar_is_opened();
bool bar_is_monitor(Monitor *m);
bool bar_is_window(xcb_window_t w);
void bar_show();
void bar_hide();
void bar_display_wmstatus(int mtags[32], int mtagset, char *cname, int ctagset);
void bar_display_systatus();
void bar_close();

#endif
