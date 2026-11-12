#ifndef CHRONOS_SCREEN_H
#define CHRONOS_SCREEN_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum {
    A_NORMAL = 0,
    A_BOLD,
    A_DIM,
    A_ACCENT,
    A_GOOD,
    A_WARN,
    A_BAD,
    A_INVERSE
};

typedef struct Screen Screen;

Screen *screen_create(int w, int h);
void    screen_destroy(Screen *s);

void screen_resize(Screen *s, int w, int h);
void screen_begin(Screen *s);
void screen_flush(Screen *s);
void screen_dump(const Screen *s, FILE *out);

int screen_w(const Screen *s);
int screen_h(const Screen *s);
int screen_text_width(const char *text);

void screen_puts(Screen *s, int x, int y, const char *text);
void screen_puts_a(Screen *s, int x, int y, const char *text, int attr);
void screen_rect_a(Screen *s, int x, int y, int w, int h, char ch, int attr);

#endif