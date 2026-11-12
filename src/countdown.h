#ifndef CHRONOS_COUNTDOWN_H
#define CHRONOS_COUNTDOWN_H

#include <stdint.h>

#include "history.h"
#include "screen.h"

typedef enum {
    CD_SETUP = 0,
    CD_RUNNING,
    CD_PAUSED,
    CD_DONE
} CdPhase;

typedef struct {
    CdPhase  phase;
    int      h, m, s;
    int      field;
    uint64_t duration_ns;
    uint64_t start_ns;
    uint64_t remain_ns;
} Countdown;

void     cd_init(Countdown *cd);
void     cd_tick(Countdown *cd, uint64_t now, History *hist);
int      cd_start(Countdown *cd, uint64_t now);
void     cd_pause(Countdown *cd, uint64_t now);
void     cd_resume(Countdown *cd, uint64_t now);
void     cd_reset(Countdown *cd);
void     cd_adjust(Countdown *cd, int delta);
uint64_t cd_remaining_ns(const Countdown *cd, uint64_t now);
int      cd_is_running(const Countdown *cd);
void     cd_render(Screen *s, const Countdown *cd, uint64_t now);

#endif