#ifndef CHRONOS_UI_H
#define CHRONOS_UI_H

#include <stddef.h>
#include <stdint.h>

#include "screen.h"

void ui_format_duration(uint64_t ns, char *buf, size_t buflen);

int  ui_big_time_width(const char *text);
void ui_big_time(Screen *s, int x, int y, const char *text, int attr);

void ui_box(Screen *s, int x, int y, int w, int h, const char *title, int attr);
void ui_rule(Screen *s, int x, int y, int w, int attr);
void ui_center(Screen *s, int y, const char *text, int attr);
void ui_progress(Screen *s, int x, int y, int w, double frac, int attr);
void ui_top_bar(Screen *s, const char *section, uint64_t now_ns);
void ui_footer(Screen *s, const char *text);

#endif