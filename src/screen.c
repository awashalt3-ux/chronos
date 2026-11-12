#include "screen.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t cp;
    uint8_t  attr;
} Cell;

struct Screen {
    int   w, h;
    int   pw, ph;
    Cell *cur;
    Cell *prev;
    char *buf;
    size_t len;
    size_t cap;
};

static int utf8_decode(const unsigned char *p, uint32_t *cp)
{
    unsigned char c = p[0];

    if (c < 0x80) {
        *cp = c;
        return 1;
    }
    if ((c & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        *cp = ((uint32_t)(c & 0x1F) << 6) | (uint32_t)(p[1] & 0x3F);
        return 2;
    }
    if ((c & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (uint32_t)(p[2] & 0x3F);
        return 3;
    }
    if ((c & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
        *cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12) |
              ((uint32_t)(p[2] & 0x3F) << 6) | (uint32_t)(p[3] & 0x3F);
        return 4;
    }
    *cp = '?';
    return 1;
}

static int utf8_emit(uint32_t cp, char *out)
{
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

static const char *sgr_for(int attr)
{
    switch (attr) {
    case A_BOLD:    return "\x1b[1m";
    case A_DIM:     return "\x1b[2m";
    case A_ACCENT:  return "\x1b[36m";
    case A_GOOD:    return "\x1b[32m";
    case A_WARN:    return "\x1b[33m";
    case A_BAD:     return "\x1b[31m";
    case A_INVERSE: return "\x1b[7m";
    default:        return "\x1b[0m";
    }
}

static void buf_reserve(Screen *s, size_t extra)
{
    size_t need;
    char *nb;

    if (s->len + extra <= s->cap)
        return;
    need = s->cap ? s->cap : 4096;
    while (need < s->len + extra)
        need *= 2;
    nb = (char *)realloc(s->buf, need);
    if (!nb)
        return;
    s->buf = nb;
    s->cap = need;
}

static void buf_str(Screen *s, const char *str, size_t n)
{
    buf_reserve(s, n);
    if (s->len + n > s->cap)
        return;
    memcpy(s->buf + s->len, str, n);
    s->len += n;
}

static void buf_puts(Screen *s, const char *str)
{
    buf_str(s, str, strlen(str));
}

static void buf_printf(Screen *s, const char *fmt, ...)
{
    char tmp[128];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n > 0)
        buf_str(s, tmp, (size_t)n < sizeof tmp ? (size_t)n : sizeof tmp - 1);
}

static void alloc_cells(Screen *s)
{
    size_t cells = (size_t)(s->w * s->h);

    s->cur = (Cell *)calloc(cells, sizeof(Cell));
    s->prev = (Cell *)calloc(cells, sizeof(Cell));
}

Screen *screen_create(int w, int h)
{
    Screen *s;

    if (w < 20) w = 20;
    if (h < 8) h = 8;

    s = (Screen *)calloc(1, sizeof(Screen));
    if (!s)
        return NULL;
    s->w = s->pw = w;
    s->h = s->ph = h;
    alloc_cells(s);
    if (!s->cur || !s->prev) {
        free(s->cur);
        free(s->prev);
        free(s);
        return NULL;
    }
    return s;
}

void screen_destroy(Screen *s)
{
    if (!s)
        return;
    free(s->cur);
    free(s->prev);
    free(s->buf);
    free(s);
}

void screen_resize(Screen *s, int w, int h)
{
    if (!s)
        return;
    if (w < 20) w = 20;
    if (h < 8) h = 8;
    if (w == s->w && h == s->h)
        return;

    free(s->cur);
    free(s->prev);
    s->w = w;
    s->h = h;
    alloc_cells(s);
    s->pw = 0;
    s->ph = 0;
}

int screen_w(const Screen *s)
{
    return s ? s->w : 0;
}

int screen_h(const Screen *s)
{
    return s ? s->h : 0;
}

void screen_begin(Screen *s)
{
    size_t cells;
    size_t i;

    if (!s || !s->cur)
        return;
    cells = (size_t)(s->w * s->h);
    for (i = 0; i < cells; i++) {
        s->cur[i].cp = ' ';
        s->cur[i].attr = A_NORMAL;
    }
}

static void put_cell(Screen *s, int x, int y, uint32_t cp, int attr)
{
    if (x < 0 || y < 0 || x >= s->w || y >= s->h)
        return;
    s->cur[(size_t)y * s->w + x].cp = cp;
    s->cur[(size_t)y * s->w + x].attr = (uint8_t)attr;
}

void screen_puts_a(Screen *s, int x, int y, const char *text, int attr)
{
    const unsigned char *p;
    int cx;

    if (!s || !text || y < 0 || y >= s->h)
        return;

    p = (const unsigned char *)text;
    cx = x;

    while (*p) {
        uint32_t cp;
        int len = utf8_decode(p, &cp);
        p += len;

        if (cp == '\n') {
            y++;
            cx = x;
            if (y >= s->h)
                break;
            continue;
        }
        if (cp < 32)
            continue;
        if (cx >= 0 && cx < s->w)
            put_cell(s, cx, y, cp, attr);
        cx++;
    }
}

void screen_puts(Screen *s, int x, int y, const char *text)
{
    screen_puts_a(s, x, y, text, A_NORMAL);
}

void screen_rect_a(Screen *s, int x, int y, int w, int h, char ch, int attr)
{
    int i, j;
    uint32_t cp = (uint32_t)(unsigned char)ch;

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++)
            put_cell(s, x + i, y + j, cp, attr);
    }
}

int screen_text_width(const char *text)
{
    const unsigned char *p = (const unsigned char *)text;
    int n = 0;

    if (!p)
        return 0;
    while (*p) {
        uint32_t cp;
        int len = utf8_decode(p, &cp);
        p += len;
        if (cp == '\n')
            break;
        if (cp >= 32)
            n++;
    }
    return n;
}

void screen_flush(Screen *s)
{
    int x, y;

    if (!s || !s->cur || s->w <= 0 || s->h <= 0)
        return;

    s->len = 0;

    if (s->pw != s->w || s->ph != s->h) {
        buf_puts(s, "\x1b[?7l\x1b[2J");
        s->pw = s->w;
        s->ph = s->h;
        memset(s->prev, 0, (size_t)(s->w * s->h) * sizeof(Cell));
    }

    for (y = 0; y < s->h; y++) {
        Cell *row = s->cur + (size_t)y * s->w;
        Cell *prow = s->prev + (size_t)y * s->w;

        x = 0;
        while (x < s->w) {
            int start, end, i, cur_attr = -1;

            if (row[x].cp == prow[x].cp && row[x].attr == prow[x].attr) {
                x++;
                continue;
            }
            start = x;
            while (x < s->w && (row[x].cp != prow[x].cp || row[x].attr != prow[x].attr))
                x++;
            end = x;

            buf_printf(s, "\x1b[%d;%dH", y + 1, start + 1);
            for (i = start; i < end; i++) {
                char b[8];
                int nb;

                if ((int)row[i].attr != cur_attr) {
                    cur_attr = row[i].attr;
                    buf_puts(s, sgr_for(cur_attr));
                }
                nb = utf8_emit(row[i].cp, b);
                buf_str(s, b, (size_t)nb);
                prow[i] = row[i];
            }
        }
    }

    buf_puts(s, "\x1b[0m\x1b[?7h");
    fwrite(s->buf, 1, s->len, stdout);
    fflush(stdout);
}

void screen_dump(const Screen *s, FILE *out)
{
    int x, y;

    if (!s || !out)
        return;
    for (y = 0; y < s->h; y++) {
        const Cell *row = s->cur + (size_t)y * s->w;
        for (x = 0; x < s->w; x++) {
            char b[8];
            int nb = utf8_emit(row[x].cp, b);
            fwrite(b, 1, (size_t)nb, out);
        }
        fputc('\n', out);
    }
    fflush(out);
}