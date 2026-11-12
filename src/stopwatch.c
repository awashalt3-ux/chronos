#include "stopwatch.h"

#include <stdio.h>
#include <string.h>

#include "ui.h"

void sw_init(Stopwatch *sw)
{
    memset(sw, 0, sizeof *sw);
}

uint64_t sw_elapsed_ns(const Stopwatch *sw, uint64_t now)
{
    uint64_t el;

    if (!sw)
        return 0;
    if (!sw->running)
        return sw->accum_ns;
    el = now - sw->start_ns;
    return sw->accum_ns + el;
}

void sw_toggle(Stopwatch *sw, uint64_t now)
{
    if (!sw)
        return;
    if (sw->running) {
        sw->accum_ns += now - sw->start_ns;
        sw->running = 0;
    } else {
        sw->start_ns = now;
        sw->running = 1;
    }
}

void sw_lap(Stopwatch *sw, uint64_t now)
{
    if (!sw || !sw->running || sw->laps_n >= SW_MAX_LAPS)
        return;
    sw->laps[sw->laps_n++] = sw_elapsed_ns(sw, now);
}

void sw_reset(Stopwatch *sw)
{
    if (!sw)
        return;
    memset(sw, 0, sizeof *sw);
}

void sw_render(Screen *s, const Stopwatch *sw, uint64_t now)
{
    int w = screen_w(s);
    int h = screen_h(s);
    uint64_t el = sw_elapsed_ns(sw, now);
    char t[40];
    int ty = 4;
    int big;

    ui_top_bar(s, "Stopwatch", now);
    ui_format_duration(el, t, sizeof t);

    big = ui_big_time_width(t);
    if (w >= big + 4) {
        ui_big_time(s, (w - big) / 2, ty, t, A_ACCENT);
        ty += 5;
    } else {
        ui_center(s, ty + 1, t, A_ACCENT);
        ty += 3;
    }

    {
        const char *status = sw->running ? "RUNNING" : (el > 0 ? "PAUSED" : "READY");
        const char *hints;
        int attr = sw->running ? A_GOOD : A_DIM;

        ui_center(s, ty, status, attr);
        ty += 2;

        if (sw->running)
            hints = "Space  Pause     L  Lap     R  Reset";
        else if (el > 0)
            hints = "Space  Resume     L  Lap     R  Reset";
        else
            hints = "Space  Start     L  Lap     R  Reset";
        ui_center(s, ty, hints, A_DIM);
        ty += 2;
    }

    if (sw->laps_n == 0)
        return;

    ty++;
    if (ty >= h - 2)
        return;

    ui_rule(s, 2, ty, w - 4, A_DIM);
    ty++;

    {
        char hdr[80];
        snprintf(hdr, sizeof hdr, "%4s  %-14s  %s", "LAP", "SPLIT", "TOTAL");
        screen_puts_a(s, 2, ty, hdr, A_DIM);
        ty++;
    }

    for (int i = sw->laps_n - 1; i >= 0 && ty < h - 2; i--) {
        uint64_t total = sw->laps[i];
        uint64_t prev = (i > 0) ? sw->laps[i - 1] : 0;
        char split[40], tot[40], line[128];

        ui_format_duration(total - prev, split, sizeof split);
        ui_format_duration(total, tot, sizeof tot);
        snprintf(line, sizeof line, "%4d  %-14s  %s", i + 1, split, tot);
        screen_puts_a(s, 2, ty, line, A_NORMAL);
        ty++;
    }
}