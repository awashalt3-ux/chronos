#include "ui.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define GLYPH_W 4
#define GLYPH_H 4
#define GLYPH_ADV 5
#define GLYPH_ROWS 8

static const char *G_0[GLYPH_ROWS] = {".##.", "#..#", "#..#", "#..#", "#..#", "#..#", "#..#", ".##."};
static const char *G_1[GLYPH_ROWS] = {"..#.", ".##.", "..#.", "..#.", "..#.", "..#.", "..#.", "####"};
static const char *G_2[GLYPH_ROWS] = {".##.", "#..#", "...#", "...#", "..#.", ".#..", "#...", "####"};
static const char *G_3[GLYPH_ROWS] = {".##.", "#..#", "...#", "..##", "...#", "...#", "#..#", ".##."};
static const char *G_4[GLYPH_ROWS] = {"...#", "..##", ".#.#", "#..#", "####", "...#", "...#", "...#"};
static const char *G_5[GLYPH_ROWS] = {"####", "#...", "#...", "####", "...#", "...#", "#..#", ".##."};
static const char *G_6[GLYPH_ROWS] = {".##.", "#..#", "#...", "####", "#..#", "#..#", "#..#", ".##."};
static const char *G_7[GLYPH_ROWS] = {"####", "...#", "...#", "..#.", "..#.", ".#..", ".#..", ".#.."};
static const char *G_8[GLYPH_ROWS] = {".##.", "#..#", "#..#", ".##.", ".##.", "#..#", "#..#", ".##."};
static const char *G_9[GLYPH_ROWS] = {".##.", "#..#", "#..#", ".###", "...#", "...#", "#..#", ".##."};
static const char *G_COLON[GLYPH_ROWS] = {"....", "....", ".##.", ".##.", "....", "....", ".##.", ".##."};
static const char *G_DOT[GLYPH_ROWS] = {"....", "....", "....", "....", "....", "....", ".##.", ".##."};

static const char *const *glyph_for(char c)
{
    switch (c) {
    case '0': return G_0;
    case '1': return G_1;
    case '2': return G_2;
    case '3': return G_3;
    case '4': return G_4;
    case '5': return G_5;
    case '6': return G_6;
    case '7': return G_7;
    case '8': return G_8;
    case '9': return G_9;
    case ':': return G_COLON;
    case '.': return G_DOT;
    default:  return NULL;
    }
}

void ui_format_duration(uint64_t ns, char *buf, size_t buflen)
{
    uint64_t total_ms = ns / 1000000ULL;
    uint64_t ms = total_ms % 1000ULL;
    uint64_t total_s = total_ms / 1000ULL;
    uint64_t s = total_s % 60ULL;
    uint64_t total_m = total_s / 60ULL;
    uint64_t m = total_m % 60ULL;
    uint64_t h = total_m / 60ULL;

    if (!buf || buflen == 0)
        return;
    snprintf(buf, buflen, "%02llu:%02llu:%02llu.%03llu",
             (unsigned long long)h, (unsigned long long)m,
             (unsigned long long)s, (unsigned long long)ms);
}

int ui_big_time_width(const char *text)
{
    int n = 0;

    if (!text)
        return 0;
    while (*text) {
        if (*text != '\n')
            n++;
        text++;
    }
    return n > 0 ? n * GLYPH_ADV - 1 : 0;
}

void ui_big_time(Screen *s, int x, int y, const char *text, int attr)
{
    int cx = x;
    const char *p;

    if (!s || !text)
        return;

    for (p = text; *p; p++) {
        const char *const *g = glyph_for(*p);
        int r;

        if (!g) {
            cx += GLYPH_ADV;
            continue;
        }
        for (r = 0; r < GLYPH_H; r++) {
            int k;

            for (k = 0; k < GLYPH_W; k++) {
                int top = (g[r * 2][k] == '#');
                int bot = (g[r * 2 + 1][k] == '#');
                const char *ch = top ? (bot ? "\xE2\x96\x88" : "\xE2\x96\x80")
                                     : (bot ? "\xE2\x96\x84" : " ");

                if (top || bot)
                    screen_puts_a(s, cx + k, y + r, ch, attr);
            }
        }
        cx += GLYPH_ADV;
    }
}

void ui_rule(Screen *s, int x, int y, int w, int attr)
{
    int i;

    for (i = 0; i < w; i++)
        screen_puts_a(s, x + i, y, "─", attr);
}

void ui_box(Screen *s, int x, int y, int w, int h, const char *title, int attr)
{
    int i, j;

    if (!s || w < 2 || h < 2)
        return;

    screen_puts_a(s, x, y, "┌", attr);
    screen_puts_a(s, x + w - 1, y, "┐", attr);
    screen_puts_a(s, x, y + h - 1, "└", attr);
    screen_puts_a(s, x + w - 1, y + h - 1, "┘", attr);

    for (i = 1; i < w - 1; i++) {
        screen_puts_a(s, x + i, y, "─", attr);
        screen_puts_a(s, x + i, y + h - 1, "─", attr);
    }
    for (j = 1; j < h - 1; j++) {
        screen_puts_a(s, x, y + j, "│", attr);
        screen_puts_a(s, x + w - 1, y + j, "│", attr);
    }

    if (title && *title) {
        int tw = screen_text_width(title);
        int tx = x + (w - tw) / 2;
        char line[128];

        if (tw + 4 > w)
            return;
        snprintf(line, sizeof line, " %s ", title);
        screen_puts_a(s, tx - 1, y, line, A_BOLD);
    }
}

void ui_center(Screen *s, int y, const char *text, int attr)
{
    int w = screen_w(s);
    int tw = screen_text_width(text);

    screen_puts_a(s, (w - tw) / 2, y, text, attr);
}

void ui_progress(Screen *s, int x, int y, int w, double frac, int attr)
{
    int inner, fill, i;

    if (!s || w < 3)
        return;
    if (frac < 0.0)
        frac = 0.0;
    if (frac > 1.0)
        frac = 1.0;

    screen_puts_a(s, x, y, "[", A_DIM);
    inner = w - 2;
    fill = (int)(frac * (double)inner + 0.5);
    if (fill > inner)
        fill = inner;
    for (i = 0; i < inner; i++)
        screen_puts_a(s, x + 1 + i, y, (i < fill) ? "\xE2\x96\x88" : "\xE2\x96\x91", (i < fill) ? attr : A_DIM);
    screen_puts_a(s, x + w - 1, y, "]", A_DIM);
}

void ui_top_bar(Screen *s, const char *section, uint64_t now_ns)
{
    int w = screen_w(s);
    int x = 1;
    char clk[16];
    time_t t = time(NULL);
    struct tm tmv;
    int cw;

    (void)now_ns;

    screen_puts_a(s, x, 0, "CHRONOS", A_ACCENT);
    x += 7;

    if (section && *section) {
        screen_puts_a(s, x, 0, "  \xC2\xB7  ", A_DIM);
        x += 5;
        screen_puts_a(s, x, 0, section, A_BOLD);
    }

    memset(&tmv, 0, sizeof tmv);
    {
        struct tm *r = localtime(&t);
        if (r)
            tmv = *r;
    }
    strftime(clk, sizeof clk, "%H:%M:%S", &tmv);

    cw = screen_text_width(clk);
    screen_puts_a(s, w - cw - 1, 0, clk, A_DIM);

    ui_rule(s, 0, 1, w, A_DIM);
}

void ui_footer(Screen *s, const char *text)
{
    int h = screen_h(s);

    screen_puts_a(s, 1, h - 1, text, A_DIM);
}