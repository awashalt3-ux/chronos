#include "platform.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <conio.h>
#  include <shellapi.h>

#  ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#    define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#  endif
#  ifndef DISABLE_NEWLINE_AUTO_RETURN
#    define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#  endif

static HANDLE   g_hout;
static DWORD    g_mode;
static int      g_mode_ok;

static HWND             g_notify_wnd;
static NOTIFYICONDATA   g_nid;
static int              g_icon_added;
static uint64_t         g_icon_until;

static LRESULT CALLBACK notify_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void notify_icon_remove(void)
{
    if (g_icon_added) {
        Shell_NotifyIconA(NIM_DELETE, &g_nid);
        g_icon_added = 0;
    }
    g_icon_until = 0;
}

static void notify_icon_ensure(void)
{
    WNDCLASSA wc;

    if (g_icon_added)
        return;

    if (!g_notify_wnd) {
        memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = notify_wndproc;
        wc.hInstance = GetModuleHandleA(NULL);
        wc.lpszClassName = "ChronosNotifyWnd";
        RegisterClassA(&wc);
        g_notify_wnd = CreateWindowExA(0, wc.lpszClassName, "Chronos", 0,
                                       0, 0, 0, 0, HWND_MESSAGE, NULL, wc.hInstance, NULL);
        if (!g_notify_wnd)
            return;
    }

    memset(&g_nid, 0, sizeof g_nid);
    g_nid.cbSize = sizeof g_nid;
    g_nid.hWnd = g_notify_wnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    g_nid.uCallbackMessage = WM_APP + 7;
    g_nid.hIcon = LoadIconA(NULL, IDI_INFORMATION);
    lstrcpynA(g_nid.szTip, "Chronos", sizeof g_nid.szTip);

    if (Shell_NotifyIconA(NIM_ADD, &g_nid))
        g_icon_added = 1;
}

static BOOL WINAPI on_ctrl_event(DWORD type)
{
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT ||
        type == CTRL_CLOSE_EVENT || type == CTRL_LOGOFF_EVENT ||
        type == CTRL_SHUTDOWN_EVENT) {
        const char *seq = "\x1b[0m\x1b[?25h\x1b[?1049l";
        DWORD written = 0;

        if (g_hout)
            WriteConsoleA(g_hout, seq, (DWORD)strlen(seq), &written, NULL);
        ExitProcess(0);
    }
    return FALSE;
}

static TermSize query_size(void)
{
    CONSOLE_SCREEN_BUFFER_INFO bi;
    TermSize t;

    t.w = 80;
    t.h = 24;
    if (g_hout && GetConsoleScreenBufferInfo(g_hout, &bi)) {
        t.w = bi.srWindow.Right - bi.srWindow.Left + 1;
        t.h = bi.srWindow.Bottom - bi.srWindow.Top + 1;
    }
    if (t.w < 20) t.w = 20;
    if (t.h < 8) t.h = 8;
    return t;
}

int plat_init(void)
{
    g_hout = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    if (g_hout && GetConsoleMode(g_hout, &g_mode)) {
        DWORD mode = g_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
        if (SetConsoleMode(g_hout, mode))
            g_mode_ok = 1;
    }

    {
        HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
        DWORD imode;

        if (hin && GetConsoleMode(hin, &imode))
            SetConsoleMode(hin, imode & ~(DWORD)ENABLE_PROCESSED_INPUT);
    }

    SetConsoleCtrlHandler(on_ctrl_event, TRUE);

    fputs("\x1b[?1049h", stdout);
    fputs("\x1b[?25l", stdout);
    fputs("\x1b[2J\x1b[H", stdout);
    fflush(stdout);
    return 1;
}

void plat_shutdown(void)
{
    notify_icon_remove();
    fputs("\x1b[0m\x1b[?25h\x1b[?1049l", stdout);
    fflush(stdout);
    if (g_mode_ok && g_hout)
        SetConsoleMode(g_hout, g_mode);
}

TermSize plat_size(void)
{
    return query_size();
}

uint64_t plat_mono_ns(void)
{
    static LARGE_INTEGER freq;
    static int init;
    LARGE_INTEGER c;

    if (!init) {
        QueryPerformanceFrequency(&freq);
        init = 1;
    }
    QueryPerformanceCounter(&c);
    if (freq.QuadPart <= 0)
        return 0;
    return (uint64_t)(((long double)c.QuadPart * 1000000000.0L) / (long double)freq.QuadPart);
}

void plat_sleep_ms(unsigned ms)
{
    Sleep(ms);
}

void plat_beep(void)
{
    fputs("\a", stdout);
    fflush(stdout);
}

void plat_notify(const char *title, const char *message)
{
    notify_icon_ensure();
    if (!g_icon_added)
        return;

    g_nid.uFlags = NIF_INFO;
    lstrcpynA(g_nid.szInfoTitle, (title && *title) ? title : "Chronos", sizeof g_nid.szInfoTitle);
    lstrcpynA(g_nid.szInfo, (message && *message) ? message : "", sizeof g_nid.szInfo);
    g_nid.dwInfoFlags = NIIF_INFO;
    g_nid.uTimeout = 5000;
    Shell_NotifyIconA(NIM_MODIFY, &g_nid);

    g_icon_until = plat_mono_ns() + 6000ULL * 1000000ULL;
}

void plat_pump(void)
{
    if (g_icon_added && g_icon_until && plat_mono_ns() >= g_icon_until)
        notify_icon_remove();
}

static int map_extended(int c)
{
    switch (c) {
    case 72: return KEY_UP;
    case 80: return KEY_DOWN;
    case 75: return KEY_LEFT;
    case 77: return KEY_RIGHT;
    case 71: return KEY_HOME;
    case 79: return KEY_END;
    case 73: return KEY_PGUP;
    case 81: return KEY_PGDN;
    case 83: return KEY_DEL;
    default: return KEY_NONE;
    }
}

static int read_key_raw(void)
{
    int c = _getch();

    if (c == 0 || c == 224) {
        int c2 = _getch();
        return map_extended(c2);
    }
    if (c == 13 || c == 10)
        return KEY_ENTER;
    if (c == 8 || c == 127)
        return KEY_BS;
    if (c == 9)
        return KEY_TAB;
    return c;
}

int key_poll(long timeout_ms)
{
    uint64_t start = plat_mono_ns();

    for (;;) {
        if (_kbhit())
            return read_key_raw();

        if (timeout_ms >= 0) {
            uint64_t now = plat_mono_ns();
            if (now - start >= (uint64_t)timeout_ms * 1000000ULL)
                return KEY_NONE;
        }
        Sleep(5);
    }
}

#else /* POSIX */

#  include <errno.h>
#  include <poll.h>
#  include <signal.h>
#  include <stdlib.h>
#  include <sys/ioctl.h>
#  include <termios.h>
#  include <time.h>
#  include <unistd.h>

static struct termios       g_orig;
static int                  g_raw;
static volatile sig_atomic_t g_winch;

static void on_winch(int sig)
{
    (void)sig;
    g_winch = 1;
}

static TermSize query_size(void)
{
    struct winsize ws;
    TermSize t;

    t.w = 80;
    t.h = 24;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        t.w = ws.ws_col;
        t.h = ws.ws_row;
    }
    return t;
}

int plat_init(void)
{
    struct termios t;

    if (!isatty(STDIN_FILENO))
        return 0;

    if (tcgetattr(STDIN_FILENO, &g_orig) == 0) {
        t = g_orig;
        t.c_lflag &= ~(tcflag_t)(ICANON | ECHO | ISIG);
        t.c_iflag &= ~(tcflag_t)(IXON | ICRNL);
        t.c_cc[VMIN] = 0;
        t.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &t) == 0)
            g_raw = 1;
    }

    signal(SIGWINCH, on_winch);
    fputs("\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H", stdout);
    fflush(stdout);
    return 1;
}

void plat_shutdown(void)
{
    fputs("\x1b[0m\x1b[?25h\x1b[?1049l", stdout);
    fflush(stdout);
    if (g_raw)
        tcsetattr(STDIN_FILENO, TCSANOW, &g_orig);
}

TermSize plat_size(void)
{
    return query_size();
}

uint64_t plat_mono_ns(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

void plat_sleep_ms(unsigned ms)
{
    struct timespec ts;

    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
        ;
}

void plat_beep(void)
{
    fputs("\a", stdout);
    fflush(stdout);
}

void plat_notify(const char *title, const char *message)
{
    char cmd[512];
    char a[128], b[256];
    size_t i;

    if (!getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY"))
        goto beep;

    for (i = 0; title && title[i] && i + 1 < sizeof a; i++)
        a[i] = (title[i] == '\'') ? ' ' : title[i];
    a[i] = '\0';
    for (i = 0; message && message[i] && i + 1 < sizeof b; i++)
        b[i] = (message[i] == '\'') ? ' ' : message[i];
    b[i] = '\0';

    snprintf(cmd, sizeof cmd, "notify-send '%s' '%s' >/dev/null 2>&1 &", a, b);
    if (system(cmd) == 0)
        return;

beep:
    plat_beep();
}

void plat_pump(void)
{
}

static int decode_escape(void)
{
    unsigned char seq[8];
    int n = 0;

    while (n < 7) {
        struct pollfd p;
        unsigned char ch;

        p.fd = STDIN_FILENO;
        p.events = POLLIN;
        if (poll(&p, 1, 10) <= 0)
            break;
        if (read(STDIN_FILENO, &ch, 1) != 1)
            break;
        seq[n++] = ch;
        if (n >= 2 && ch >= 0x40 && ch <= 0x7E)
            break;
    }

    if (n >= 3 && seq[0] == '[') {
        if (seq[1] == 'A') return KEY_UP;
        if (seq[1] == 'B') return KEY_DOWN;
        if (seq[1] == 'C') return KEY_RIGHT;
        if (seq[1] == 'D') return KEY_LEFT;
        if (seq[1] == 'H') return KEY_HOME;
        if (seq[1] == 'F') return KEY_END;
        if (seq[1] == '3' && seq[2] == '~') return KEY_DEL;
        if (seq[1] == '5' && seq[2] == '~') return KEY_PGUP;
        if (seq[1] == '6' && seq[2] == '~') return KEY_PGDN;
    }
    return KEY_ESC;
}

int key_poll(long timeout_ms)
{
    struct pollfd p;
    unsigned char c;
    int r;

    if (g_winch) {
        g_winch = 0;
        return KEY_NONE;
    }

    p.fd = STDIN_FILENO;
    p.events = POLLIN;
    r = poll(&p, 1, (int)timeout_ms);
    if (r <= 0)
        return KEY_NONE;

    if (read(STDIN_FILENO, &c, 1) != 1)
        return KEY_NONE;

    if (c == 27)
        return decode_escape();
    if (c == 13 || c == 10)
        return KEY_ENTER;
    if (c == 8 || c == 127)
        return KEY_BS;
    if (c == 9)
        return KEY_TAB;
    if (c == 3)
        return 3;
    return c;
}

#endif