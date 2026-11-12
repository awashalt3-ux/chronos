#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "countdown.h"
#include "history.h"
#include "platform.h"
#include "screen.h"
#include "stopwatch.h"
#include "ui.h"

#define VERSION "1.0"

typedef enum {
    SCR_MENU,
    SCR_STOPWATCH,
    SCR_TIMER,
    SCR_HIST
} ScreenId;

typedef struct {
    ScreenId  id;
    int       menu_sel;
    int       hist_sel;
    int       confirm_clear;
    uint64_t  confirm_until;
    char      toast[96];
    uint64_t  toast_until;
    Stopwatch sw;
    Countdown cd;
    History   hist;
    int       quit;
} App;

static void toastf(App *a, uint64_t now, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(a->toast, sizeof a->toast, fmt, ap);
    va_end(ap);
    a->toast_until = now + 2500ULL * 1000000ULL;
}

static const char *MENU_LABELS[] = {
    "Stopwatch",
    "Timer",
    "History",
    "Quit"
};

#define MENU_COUNT 4

static const char *BANNER[] = {
    " ██████╗██╗  ██╗██████╗  ██████╗ ███╗   ██╗ ██████╗ ███████╗",
    "██╔════╝██║  ██║██╔══██╗██╔═══██╗████╗  ██║██╔═══██╗██╔════╝",
    "██║     ███████║██████╔╝██║   ██║██╔██╗ ██║██║   ██║███████╗",
    "██║     ██╔══██║██╔══██╗██║   ██║██║╚██╗██║██║   ██║╚════██║",
    "╚██████╗██║  ██║██║  ██║╚██████╔╝██║ ╚████║╚██████╔╝███████║",
    " ╚═════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═══╝ ╚═════╝ ╚══════╝"
};

#define BANNER_H 6

static void render_menu(App *a, Screen *s, uint64_t now)
{
    int w = screen_w(s);
    int h = screen_h(s);
    int y = 3;
    int i;
    char label[64];

    ui_top_bar(s, NULL, now);

    if (w >= 62) {
        int bw = screen_text_width(BANNER[0]);
        int bx = (w - bw) / 2;
        int block = BANNER_H + 1 + 1 + 1 + MENU_COUNT;

        y = (h - block) / 2;
        if (y < 3)
            y = 3;
        for (i = 0; i < BANNER_H; i++)
            screen_puts_a(s, bx, y + i, BANNER[i], A_ACCENT);
        y += BANNER_H + 1;
    } else {
        int block = 1 + 1 + 1 + MENU_COUNT;

        y = (h - block) / 2;
        if (y < 3)
            y = 3;
        ui_center(s, y, "C H R O N O S", A_ACCENT);
        y += 2;
    }

    ui_center(s, y, "terminal timer", A_DIM);
    y += 2;

    for (i = 0; i < MENU_COUNT; i++) {
        int sel = (i == a->menu_sel);
        int lw;

        if (i == 2)
            snprintf(label, sizeof label, "%s  (%d)", MENU_LABELS[i], a->hist.n);
        else
            snprintf(label, sizeof label, "%s", MENU_LABELS[i]);

        lw = screen_text_width(label);
        if (sel) {
            screen_puts_a(s, (w - lw) / 2 - 2, y + i, "▸", A_ACCENT);
            screen_puts_a(s, (w - lw) / 2, y + i, label, A_BOLD);
        } else {
            screen_puts_a(s, (w - lw) / 2, y + i, label, A_NORMAL);
        }
    }

    ui_center(s, h - 3, "↑/↓  Navigate     Enter  Select     Q  Quit", A_DIM);
    ui_footer(s, "CHRONOS v" VERSION "   ©  2026");
}

static void render_stopwatch(App *a, Screen *s, uint64_t now)
{
    sw_render(s, &a->sw, now);

    if (a->toast_until > now)
        ui_center(s, screen_h(s) - 2, a->toast, A_WARN);
}

static void render_timer(App *a, Screen *s, uint64_t now)
{
    cd_render(s, &a->cd, now);

    if (a->toast_until > now)
        ui_center(s, screen_h(s) - 2, a->toast, A_WARN);
}

static void render_hist(App *a, Screen *s, uint64_t now)
{
    int w = screen_w(s);
    int h = screen_h(s);
    int y = 3;
    int rows, top, r;
    char hdr[80];

    ui_top_bar(s, "History", now);

    if (a->hist.n == 0) {
        ui_center(s, h / 2, "No records yet.", A_DIM);
        ui_center(s, h / 2 + 1, "Start a stopwatch or a timer.", A_DIM);
        ui_footer(s, "Esc  Back");
        return;
    }

    snprintf(hdr, sizeof hdr, "%-10s  %-14s  %s", "TYPE", "DURATION", "DATE");
    screen_puts_a(s, 2, y, hdr, A_DIM);
    y++;
    ui_rule(s, 2, y, w - 4, A_DIM);
    y++;

    rows = h - y - 3;
    if (rows < 1)
        rows = 1;
    top = 0;
    if (a->hist_sel >= rows)
        top = a->hist_sel - rows + 1;

    for (r = 0; r < rows && top + r < a->hist.n; r++) {
        int idx = top + r;
        const HistEntry *e = &a->hist.v[idx];
        char dur[40], date[32], line[96];
        int sel = (idx == a->hist_sel);

        ui_format_duration(e->dur_ms * 1000000ULL, dur, sizeof dur);
        history_entry_date(e, date, sizeof date);
        snprintf(line, sizeof line, "%-10s  %-14s  %s",
                 history_kind_label(e->kind), dur, date);

        if (sel) {
            screen_puts_a(s, 0, y + r, "▸", A_ACCENT);
            screen_puts_a(s, 2, y + r, line, A_ACCENT);
        } else {
            screen_puts_a(s, 2, y + r, line, A_NORMAL);
        }
    }

    if (a->confirm_clear) {
        ui_footer(s, "C  Confirm deletion     Esc  Cancel");
        ui_center(s, h - 2, "▲  Clear all history?", A_BAD);
    } else {
        ui_footer(s, "↑/↓  Navigate     Del  Delete     C  Clear     Esc  Back");
    }
}

static void render(App *a, Screen *s, uint64_t now)
{
    if (a->toast_until && a->toast_until <= now)
        a->toast_until = 0;

    switch (a->id) {
    case SCR_MENU:     render_menu(a, s, now); break;
    case SCR_STOPWATCH:   render_stopwatch(a, s, now); break;
    case SCR_TIMER: render_timer(a, s, now); break;
    case SCR_HIST:     render_hist(a, s, now); break;
    }
}

static void key_menu(App *a, int key)
{
    if (key == KEY_UP)
        a->menu_sel = (a->menu_sel + MENU_COUNT - 1) % MENU_COUNT;
    else if (key == KEY_DOWN)
        a->menu_sel = (a->menu_sel + 1) % MENU_COUNT;
    else if (key == KEY_ENTER || key == KEY_SPACE) {
        switch (a->menu_sel) {
        case 0: a->id = SCR_STOPWATCH; break;
        case 1: a->id = SCR_TIMER; break;
        case 2: a->id = SCR_HIST; a->hist_sel = 0; a->confirm_clear = 0; break;
        default: a->quit = 1; break;
        }
    } else if (key == 'q' || key == 'Q' || key == KEY_ESC || key == 3) {
        a->quit = 1;
    }
}

static void key_stopwatch(App *a, int key)
{
    uint64_t now = plat_mono_ns();

    if (key == KEY_ESC) {
        a->id = SCR_MENU;
        return;
    }
    if (key == KEY_SPACE) {
        sw_toggle(&a->sw, now);
        return;
    }
    if (key == 'l' || key == 'L') {
        sw_lap(&a->sw, now);
        return;
    }
    if (key == 'r' || key == 'R') {
        uint64_t el = sw_elapsed_ns(&a->sw, now);
        if (el > 0) {
            history_append(&a->hist, HIST_STOPWATCH, el / 1000000ULL);
            toastf(a, now, "Session saved to history");
        }
        sw_reset(&a->sw);
    }
}

static void key_timer(App *a, int key)
{
    uint64_t now = plat_mono_ns();

    switch (a->cd.phase) {
    case CD_SETUP:
        if (key == KEY_ESC)
            a->id = SCR_MENU;
        else if (key == KEY_UP)
            cd_adjust(&a->cd, 1);
        else if (key == KEY_DOWN)
            cd_adjust(&a->cd, -1);
        else if (key == KEY_LEFT)
            a->cd.field = (a->cd.field + 2) % 3;
        else if (key == KEY_RIGHT)
            a->cd.field = (a->cd.field + 1) % 3;
        else if (key == KEY_ENTER || key == KEY_SPACE) {
            if (!cd_start(&a->cd, now))
                toastf(a, now, "Set a non-zero duration first");
        }
        break;

    case CD_RUNNING:
        if (key == KEY_SPACE)
            cd_pause(&a->cd, now);
        else if (key == 'r' || key == 'R')
            cd_reset(&a->cd);
        else if (key == KEY_ESC)
            a->id = SCR_MENU;
        break;

    case CD_PAUSED:
        if (key == KEY_SPACE)
            cd_resume(&a->cd, now);
        else if (key == 'r' || key == 'R')
            cd_reset(&a->cd);
        else if (key == KEY_ESC)
            a->id = SCR_MENU;
        break;

    case CD_DONE:
        if (key == KEY_ESC)
            a->id = SCR_MENU;
        else
            cd_reset(&a->cd);
        break;
    }
}

static void key_hist(App *a, int key)
{
    if (a->confirm_clear) {
        if (key == 'c' || key == 'C') {
            history_clear(&a->hist);
            a->hist_sel = 0;
            a->confirm_clear = 0;
            toastf(a, plat_mono_ns(), "History cleared");
        } else {
            a->confirm_clear = 0;
        }
        if (key == KEY_ESC)
            a->id = SCR_MENU;
        return;
    }

    if (key == KEY_ESC) {
        a->id = SCR_MENU;
        return;
    }
    if (a->hist.n == 0)
        return;

    if (key == KEY_UP)
        a->hist_sel = (a->hist_sel + a->hist.n - 1) % a->hist.n;
    else if (key == KEY_DOWN)
        a->hist_sel = (a->hist_sel + 1) % a->hist.n;
    else if (key == KEY_DEL) {
        history_remove(&a->hist, a->hist_sel);
        if (a->hist.n == 0)
            a->hist_sel = 0;
        else if (a->hist_sel >= a->hist.n)
            a->hist_sel = a->hist.n - 1;
    } else if (key == 'c' || key == 'C') {
        a->confirm_clear = 1;
        a->confirm_until = plat_mono_ns() + 3000ULL * 1000000ULL;
    }
}

static void dispatch(App *a, int key)
{
    if (key == 3) {
        a->quit = 1;
        return;
    }
    switch (a->id) {
    case SCR_MENU:     key_menu(a, key); break;
    case SCR_STOPWATCH:   key_stopwatch(a, key); break;
    case SCR_TIMER: key_timer(a, key); break;
    case SCR_HIST:     key_hist(a, key); break;
    }
}

static int preview_mode(int cols, int rows)
{
    Screen *s = screen_create(cols, rows);
    App a;
    uint64_t now = 0;

    if (!s)
        return 1;

    memset(&a, 0, sizeof a);
    sw_init(&a.sw);
    cd_init(&a.cd);
    history_init(&a.hist);

    a.sw.running = 1;
    a.sw.start_ns = 0;
    a.sw.accum_ns = 0;
    a.sw.laps[0] = 3210000000ULL;
    a.sw.laps[1] = 7450000000ULL;
    a.sw.laps_n = 2;

    a.hist.v = (HistEntry *)calloc(4, sizeof(HistEntry));
    a.hist.cap = 4;
    a.hist.n = 4;
    a.hist.v[0].kind = HIST_TIMER;
    a.hist.v[0].dur_ms = 300000ULL;
    a.hist.v[0].epoch = 1700000000;
    a.hist.v[1].kind = HIST_STOPWATCH;
    a.hist.v[1].dur_ms = 83456ULL;
    a.hist.v[1].epoch = 1700000100;
    a.hist.v[2].kind = HIST_STOPWATCH;
    a.hist.v[2].dur_ms = 1523450ULL;
    a.hist.v[2].epoch = 1700000200;
    a.hist.v[3].kind = HIST_TIMER;
    a.hist.v[3].dur_ms = 45000ULL;
    a.hist.v[3].epoch = 1700000300;

    a.id = SCR_MENU;
    screen_begin(s);
    render_menu(&a, s, now);
    screen_dump(s, stdout);
    fputs("\n", stdout);

    a.id = SCR_STOPWATCH;
    screen_begin(s);
    render_stopwatch(&a, s, now);
    screen_dump(s, stdout);
    fputs("\n", stdout);

    a.id = SCR_TIMER;
    a.cd.phase = CD_SETUP;
    screen_begin(s);
    render_timer(&a, s, now);
    screen_dump(s, stdout);
    fputs("\n", stdout);

    a.cd.phase = CD_RUNNING;
    a.cd.h = 0;
    a.cd.m = 5;
    a.cd.s = 0;
    a.cd.duration_ns = 300000000000ULL;
    a.cd.remain_ns = 300000000000ULL;
    a.cd.start_ns = 0;
    screen_begin(s);
    render_timer(&a, s, 75000000000ULL);
    screen_dump(s, stdout);
    fputs("\n", stdout);

    a.cd.phase = CD_DONE;
    a.cd.duration_ns = 5000000000ULL;
    a.cd.remain_ns = 0;
    screen_begin(s);
    render_timer(&a, s, 10000000000ULL);
    screen_dump(s, stdout);
    fputs("\n", stdout);

    a.id = SCR_HIST;
    a.hist_sel = 1;
    screen_begin(s);
    render_hist(&a, s, now);
    screen_dump(s, stdout);

    free(a.hist.v);
    screen_destroy(s);
    return 0;
}

int main(int argc, char **argv)
{
    App a;
    Screen *s;

    if (argc > 1 && strcmp(argv[1], "--preview") == 0) {
        int cols = (argc > 2) ? atoi(argv[2]) : 100;
        int rows = (argc > 3) ? atoi(argv[3]) : 30;
        if (cols < 20) cols = 20;
        if (rows < 8) rows = 8;
        return preview_mode(cols, rows);
    }

    if (!plat_init()) {
        fprintf(stderr, "Chronos: not an interactive terminal.\n");
        return 1;
    }

    memset(&a, 0, sizeof a);
    sw_init(&a.sw);
    cd_init(&a.cd);
    history_load(&a.hist);

    s = screen_create(plat_size().w, plat_size().h);
    if (!s) {
        plat_shutdown();
        fprintf(stderr, "Chronos: out of memory.\n");
        return 1;
    }

    while (!a.quit) {
        TermSize ts = plat_size();
        uint64_t now;
        int key;

        plat_pump();
        screen_resize(s, ts.w, ts.h);

        now = plat_mono_ns();
        cd_tick(&a.cd, now, &a.hist);

        if (a.confirm_clear && now > a.confirm_until)
            a.confirm_clear = 0;

        screen_begin(s);
        render(&a, s, now);
        screen_flush(s);

        key = key_poll(50);
        if (key != KEY_NONE)
            dispatch(&a, key);
    }

    screen_destroy(s);
    history_free(&a.hist);
    plat_shutdown();
    return 0;
}