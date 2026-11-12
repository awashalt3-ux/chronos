#ifndef CHRONOS_PLATFORM_H
#define CHRONOS_PLATFORM_H

#include <stdint.h>

typedef struct {
    int w;
    int h;
} TermSize;

enum {
    KEY_NONE = 0,
    KEY_ESC = 27,
    KEY_ENTER = 13,
    KEY_TAB = 9,
    KEY_BS = 8,
    KEY_SPACE = ' ',

    KEY_UP = 0x100,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_HOME,
    KEY_END,
    KEY_PGUP,
    KEY_PGDN,
    KEY_DEL
};

int      plat_init(void);
void     plat_shutdown(void);
TermSize plat_size(void);
uint64_t plat_mono_ns(void);
void     plat_sleep_ms(unsigned ms);
void     plat_beep(void);
void     plat_notify(const char *title, const char *message);
void     plat_pump(void);

int key_poll(long timeout_ms);

#endif