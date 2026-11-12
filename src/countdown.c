#include "countdown.h"

#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "ui.h"

void cd_init(Countdown *cd)
{
    memset(cd, 0, sizeof *cd);
    cd->phase = CD_SETUP;
    cd->field = 1;
    cd->m = 5;
}

void cd_tick(Countdown *cd, uint64_t now, History *hist)
{
    if (!cd || cd->phase != CD_RUNNING)
        return;
    if (cd_remaining_ns(cd, now) > 0)
        return;

    cd->remain_ns = 0;
    cd->phase = CD_DONE;
    if (hist)
        history_append(hist, HIST_TIMER, cd->duration_ns / 1000000ULL);
    plat_notify("Chronos", "Timer finished");
    plat_beep();
    plat_sleep_ms(180);
    plat_beep();
}

int cd_start(Countdown *cd, uint64_t now)
{
    uint64_t dur;

    if (!cd)
        return 0;
    dur = ((uint64_t)cd->h * 3600ULL + (uint64_t)cd->m * 60ULL + (uint64_t)cd->s) * 1000000000ULL;
    if (dur == 0)
        return 0;

    cd->duration_ns = dur;
    cd->remain_ns = dur;
    cd->start_ns = now;
    cd->phase = CD_RUNNING;
    return 1;
}

void cd_pause(Countdown *cd, uint64_t now)
{
    if (!cd || cd->phase != CD_RUNNING)
        return;
    cd->remain_ns = cd_remaining_ns(cd, now);
    cd->phase = CD_PAUSED;
}

void cd_resume(Countdown *cd, uint64_t now)
{
    if (!cd || cd->phase != CD_PAUSED)
        return;
    cd->start_ns = now;
    cd->phase = CD_RUNNING;
}

void cd_reset(Countdown *cd)
{
    if (!cd)
        return;
    cd->phase = CD_SETUP;
    cd->remain_ns = 0;
    cd->duration_ns = 0;
    cd->start_ns = 0;
}

void cd_adjust(Countdown *cd, int delta)
{
    int *field;
    int value;

    if (!cd)
        return;
    field = (cd->field == 0) ? &cd->h : (cd->field == 1) ? &cd->m : &cd->s;
    value = *field + delta;

    if (cd->field == 0) {
        if (value < 0)
            value = 23;
        if (value > 23)
            value = 0;
    } else {
        if (value < 0)
            value = 59;
        if (value > 59)
            value = 0;
    }
    *field = value;
}

uint64_t cd_remaining_ns(const Countdown *cd, uint64_t now)
{
    uint64_t el;

    if (!cd)
        return 0;
    if (cd->phase != CD_RUNNING)
        return cd->remain_ns;
    el = now - cd->start_ns;
    if (el >= cd->remain_ns)
        return 0;
    return cd->remain_ns - el;
}

int cd_is_running(const Countdown *cd)
{
    return cd && cd->phase == CD_RUNNING;
}

static void render_setup(Screen *s, const Countdown *cd)
{
    int w = screen_w(s);
    int h = screen_h(s);
    int y = 4;
    static const char *LABELS[3] = {"HOURS", "MINUTES", "SECONDS"};
    int vals[3];
    int bw = 16, bh = 5, gap = 4;
    int total = bw * 3 + gap * 2;
    int x0 = (w - total) / 2;
    int i;
    char tmp[3][16];

    ui_center(s, y, "Set the duration", A_BOLD);
    y += 3;

    if (x0 < 1)
        x0 = 1;

    vals[0] = cd->h;
    vals[1] = cd->m;
    vals[2] = cd->s;
    for (i = 0; i < 3; i++)
        snprintf(tmp[i], sizeof tmp[i], "%d", vals[i]);

    for (i = 0; i < 3; i++) {
        int bx = x0 + i * (bw + gap);
        int focus = (i == cd->field);
        int attr = focus ? A_ACCENT : A_DIM;
        int lw = screen_text_width(LABELS[i]);
        int vw = screen_text_width(tmp[i]);

        ui_box(s, bx, y, bw, bh, NULL, attr);
        screen_puts_a(s, bx + (bw - lw) / 2, y - 1, LABELS[i], attr);
        screen_puts_a(s, bx + (bw - vw) / 2, y + 2, tmp[i], A_BOLD);
        if (focus)
            screen_puts_a(s, bx + (bw - 1) / 2, y + bh, "▲", A_ACCENT);
    }

    y += bh + 3;
    if (y < h - 2) {
        ui_center(s, y, "↑/↓  Adjust     ←/→  Field", A_DIM);
        ui_center(s, y + 1, "Enter  Start     Esc  Back", A_DIM);
    }
}

static void render_running(Screen *s, const Countdown *cd, uint64_t now)
{
    int w = screen_w(s);
    uint64_t rem = cd_remaining_ns(cd, now);
    char t[40];
    int ty = 4;
    int bw;
    double frac;
    char line[80];
    int big;

    ui_format_duration(rem, t, sizeof t);

    big = ui_big_time_width(t);
    if (w >= big + 4) {
        ui_big_time(s, (w - big) / 2, ty, t, A_ACCENT);
        ty += 5;
    } else {
        ui_center(s, ty + 1, t, A_ACCENT);
        ty += 3;
    }

    if (cd->phase == CD_DONE) {
        int blink = (int)((now / 350000000ULL) & 1ULL);

        ui_center(s, ty, blink ? "▲  TIME'S UP  ▲" : "TIME'S UP", A_BAD);
        ty += 2;
        ui_center(s, ty, "Press any key to restart", A_DIM);
        return;
    }

    frac = 0.0;
    if (cd->duration_ns > 0)
        frac = (double)(cd->duration_ns - rem) / (double)cd->duration_ns;

    bw = (w >= 60) ? 44 : (w - 10);
    if (bw > 8) {
        int pct = (int)(frac * 100.0 + 0.5);
        int bx = (w - bw) / 2;

        ui_progress(s, bx, ty, bw, frac, A_GOOD);
        snprintf(line, sizeof line, "%d%%", pct);
        screen_puts_a(s, bx + bw + 2, ty, line, A_DIM);
        ty += 2;
    }

    {
        const char *status;
        int attr;

        if (cd->phase == CD_RUNNING) {
            status = "RUNNING";
            attr = A_GOOD;
        } else {
            status = "PAUSED";
            attr = A_WARN;
        }
        ui_center(s, ty, status, attr);
        ty += 2;
    }

    if (cd->phase == CD_RUNNING)
        ui_center(s, ty, "Space  Pause     R  Reset     Esc  Back", A_DIM);
    else
        ui_center(s, ty, "Space  Resume     R  Reset     Esc  Back", A_DIM);
}

void cd_render(Screen *s, const Countdown *cd, uint64_t now)
{
    ui_top_bar(s, "Timer", now);

    if (!cd)
        return;
    if (cd->phase == CD_SETUP)
        render_setup(s, cd);
    else
        render_running(s, cd, now);
}