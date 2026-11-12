#ifndef CHRONOS_HISTORY_H
#define CHRONOS_HISTORY_H

#include <stdint.h>

typedef enum {
    HIST_STOPWATCH = 0,
    HIST_TIMER = 1
} HistKind;

typedef struct {
    HistKind  kind;
    uint64_t  dur_ms;
    long long epoch;
} HistEntry;

typedef struct {
    HistEntry *v;
    int        n;
    int        cap;
} History;

void history_init(History *h);
void history_free(History *h);
int  history_load(History *h);
int  history_save(const History *h);
int  history_append(History *h, HistKind kind, uint64_t dur_ms);
int  history_remove(History *h, int index);
int  history_clear(History *h);

const char *history_kind_label(HistKind kind);
void        history_entry_date(const HistEntry *e, char *buf, int buflen);

#endif