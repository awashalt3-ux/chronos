#include "history.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#  include <direct.h>
#  define CH_MKDIR(p) _mkdir(p)
#else
#  include <sys/stat.h>
#  include <sys/types.h>
#  define CH_MKDIR(p) mkdir((p), 0777)
#endif

#define HISTORY_MAX 500

static int history_path(char *out, size_t n)
{
    char dir[900];
    const char *base;
    int k;

#if defined(_WIN32)
    base = getenv("LOCALAPPDATA");
    if (!base || !*base)
        base = getenv("APPDATA");
    if (!base || !*base)
        base = ".";
    k = snprintf(dir, sizeof dir, "%s\\Chronos", base);
#else
    base = getenv("XDG_DATA_HOME");
    if (base && *base)
        k = snprintf(dir, sizeof dir, "%s/Chronos", base);
    else {
        const char *home = getenv("HOME");
        if (!home || !*home)
            home = ".";
        k = snprintf(dir, sizeof dir, "%s/.local/share/Chronos", home);
    }
#endif
    if (k < 0 || (size_t)k >= sizeof dir)
        return 0;
    CH_MKDIR(dir);

#if defined(_WIN32)
    k = snprintf(out, n, "%s\\history.txt", dir);
#else
    k = snprintf(out, n, "%s/history.txt", dir);
#endif
    return (k >= 0 && (size_t)k < n) ? 1 : 0;
}

void history_init(History *h)
{
    memset(h, 0, sizeof *h);
}

void history_free(History *h)
{
    if (!h)
        return;
    free(h->v);
    h->v = NULL;
    h->n = h->cap = 0;
}

static int history_reserve(History *h, int need)
{
    int cap;
    HistEntry *nv;

    if (need <= h->cap)
        return 1;
    cap = h->cap ? h->cap : 16;
    while (cap < need)
        cap *= 2;
    nv = (HistEntry *)realloc(h->v, (size_t)cap * sizeof(HistEntry));
    if (!nv)
        return 0;
    h->v = nv;
    h->cap = cap;
    return 1;
}

int history_save(const History *h)
{
    char path[1024];
    FILE *f;
    int i;

    if (!h)
        return 0;
    if (!history_path(path, sizeof path))
        return 0;
    f = fopen(path, "wb");
    if (!f)
        return 0;
    fputs("# Chronos history v1\n", f);
    for (i = 0; i < h->n; i++) {
        const HistEntry *e = &h->v[i];
        fprintf(f, "%c %llu %lld\n",
                (e->kind == HIST_TIMER) ? 'M' : 'C',
                (unsigned long long)e->dur_ms,
                e->epoch);
    }
    fclose(f);
    return 1;
}

int history_load(History *h)
{
    char path[1024];
    FILE *f;
    char line[128];

    if (!h)
        return 0;
    history_init(h);
    if (!history_path(path, sizeof path))
        return 0;
    f = fopen(path, "rb");
    if (!f)
        return 0;

    while (fgets(line, sizeof line, f)) {
        char kind;
        unsigned long long dur;
        long long epoch;

        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;
        if (sscanf(line, " %c %llu %lld", &kind, &dur, &epoch) != 3)
            continue;
        if (kind != 'C' && kind != 'M')
            continue;
        if (!history_reserve(h, h->n + 1))
            break;
        h->v[h->n].kind = (kind == 'M') ? HIST_TIMER : HIST_STOPWATCH;
        h->v[h->n].dur_ms = (uint64_t)dur;
        h->v[h->n].epoch = epoch;
        h->n++;
    }
    fclose(f);
    return h->n;
}

int history_append(History *h, HistKind kind, uint64_t dur_ms)
{
    HistEntry e;
    int i;

    if (!h)
        return 0;
    if (!history_reserve(h, h->n + 1))
        return 0;

    if (h->n >= HISTORY_MAX) {
        h->n--;
        for (i = 0; i < h->n; i++)
            h->v[i] = h->v[i + 1];
    }

    e.kind = kind;
    e.dur_ms = dur_ms;
    e.epoch = (long long)time(NULL);

    for (i = h->n; i > 0; i--)
        h->v[i] = h->v[i - 1];
    h->v[0] = e;
    h->n++;

    history_save(h);
    return 1;
}

int history_remove(History *h, int index)
{
    int i;

    if (!h || index < 0 || index >= h->n)
        return 0;
    for (i = index; i < h->n - 1; i++)
        h->v[i] = h->v[i + 1];
    h->n--;
    history_save(h);
    return 1;
}

int history_clear(History *h)
{
    if (!h)
        return 0;
    h->n = 0;
    history_save(h);
    return 1;
}

const char *history_kind_label(HistKind kind)
{
    return kind == HIST_TIMER ? "TIMER" : "STOPWATCH";
}

void history_entry_date(const HistEntry *e, char *buf, int buflen)
{
    time_t t;
    struct tm tmv;
    struct tm *r;

    if (!buf || buflen <= 0)
        return;
    if (!e) {
        buf[0] = '\0';
        return;
    }
    t = (time_t)e->epoch;
    memset(&tmv, 0, sizeof tmv);
    r = localtime(&t);
    if (r)
        tmv = *r;
    strftime(buf, (size_t)buflen, "%Y-%m-%d %H:%M", &tmv);
}