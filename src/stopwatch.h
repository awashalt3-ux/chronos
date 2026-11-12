#ifndef CHRONOS_STOPWATCH_H
#define CHRONOS_STOPWATCH_H

#include <stdint.h>

#include "screen.h"

#define SW_MAX_LAPS 256

typedef struct {
    int      running;
    uint64_t start_ns;
    uint64_t accum_ns;
    uint64_t laps[SW_MAX_LAPS];
    int      laps_n;
} Stopwatch;

void     sw_init(Stopwatch *sw);
uint64_t sw_elapsed_ns(const Stopwatch *sw, uint64_t now);
void     sw_toggle(Stopwatch *sw, uint64_t now);
void     sw_lap(Stopwatch *sw, uint64_t now);
void     sw_reset(Stopwatch *sw);
void     sw_render(Screen *s, const Stopwatch *sw, uint64_t now);

#endif