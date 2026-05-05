// SPDX-License-Identifier: GPL-2.0-or-later
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define ESC "\033["
#define ANSI_RESET ESC "0m"
#define ANSI_BOLD ESC "1m"
#define ANSI_DIM ESC "2m"
#define FG_TEXT ESC "38;2;255;255;255m"
#define FG_ACCENT ESC "38;2;29;155;240m"
#define FG_WARN ESC "38;2;254;198;107m"
#define FG_BAD ESC "38;2;254;107;107m"
#define FG_BORDER_UNFOCUSED ESC "38;2;78;79;79m"
#define BG_APP ESC "48;2;0;5;15m"
#define BG_SIDEBAR ESC "48;2;3;8;20m"
#define BG_ACCENT ESC "48;2;29;155;240m"
#define MAIN_BASE ANSI_RESET FG_TEXT BG_APP
#define MAIN_ACCENT ANSI_RESET FG_ACCENT BG_APP
#define MAIN_ACCENT_BOLD ANSI_RESET ANSI_BOLD FG_ACCENT BG_APP
#define MAIN_DIM ANSI_RESET ANSI_DIM FG_TEXT BG_APP
#define MAIN_WARN ANSI_RESET FG_WARN BG_APP
#define MAIN_BAD ANSI_RESET FG_BAD BG_APP
#define MAIN_BORDER ANSI_RESET FG_ACCENT BG_APP
#define MAIN_BORDER_DIM ANSI_RESET FG_BORDER_UNFOCUSED BG_APP
#define MAIN_SELECTED ANSI_RESET ANSI_BOLD FG_TEXT BG_ACCENT
#define SIDEBAR_BASE ANSI_RESET FG_TEXT BG_SIDEBAR
#define SIDEBAR_ACCENT_BOLD ANSI_RESET ANSI_BOLD FG_ACCENT BG_SIDEBAR
#define SIDEBAR_DIM ANSI_RESET ANSI_DIM FG_TEXT BG_SIDEBAR
#define SIDEBAR_BORDER ANSI_RESET FG_ACCENT BG_SIDEBAR
#define ENABLE_KITTY_KBD ESC ">1u"
#define DISABLE_KITTY_KBD ESC "<u"
#define SEP_H "─"
#define SEP_V "│"
#define SEP_CROSS "┼"

static struct termios orig_termios;
static int sockfd = -1;
static struct sockaddr_un sock_addr;
static char socket_path[108];
static int running = 1;
static int need_redraw = 1;
static int classic_mode = 0;
static char last_event[128] = "ready";
static int event_count = 0;
static int connected_hint = 0;

typedef enum { EV_BUTTON, EV_MOTION } EventKind;

typedef struct {
    const char *label;
    const char *name;
    EventKind kind;
    int active;
    long long release_at_ms;
} Control;

enum {
    C_UP, C_DOWN, C_LEFT, C_RIGHT,
    C_A, C_B, C_ONE, C_TWO, C_MINUS, C_PLUS, C_HOME,
    C_IR_UP, C_IR_DOWN, C_IR_LEFT, C_IR_RIGHT,
    C_CL_UP, C_CL_DOWN, C_CL_LEFT, C_CL_RIGHT,
    C_CL_A, C_CL_B, C_CL_X, C_CL_Y, C_CL_L, C_CL_R, C_CL_ZL, C_CL_ZR, C_CL_MINUS, C_CL_PLUS,
    NCONTROLS
};

static Control controls[NCONTROLS] = {
    [C_UP] = {"D↑", "WIIMOTE_UP", EV_BUTTON, 0, 0},
    [C_DOWN] = {"D↓", "WIIMOTE_DOWN", EV_BUTTON, 0, 0},
    [C_LEFT] = {"D←", "WIIMOTE_LEFT", EV_BUTTON, 0, 0},
    [C_RIGHT] = {"D→", "WIIMOTE_RIGHT", EV_BUTTON, 0, 0},
    [C_A] = {"A", "WIIMOTE_A", EV_BUTTON, 0, 0},
    [C_B] = {"B", "WIIMOTE_B", EV_BUTTON, 0, 0},
    [C_ONE] = {"1", "WIIMOTE_1", EV_BUTTON, 0, 0},
    [C_TWO] = {"2", "WIIMOTE_2", EV_BUTTON, 0, 0},
    [C_MINUS] = {"−", "WIIMOTE_MINUS", EV_BUTTON, 0, 0},
    [C_PLUS] = {"+", "WIIMOTE_PLUS", EV_BUTTON, 0, 0},
    [C_HOME] = {"Home", "HOME", EV_BUTTON, 0, 0},
    [C_IR_UP] = {"IR↑", "IR_UP", EV_MOTION, 0, 0},
    [C_IR_DOWN] = {"IR↓", "IR_DOWN", EV_MOTION, 0, 0},
    [C_IR_LEFT] = {"IR←", "IR_LEFT", EV_MOTION, 0, 0},
    [C_IR_RIGHT] = {"IR→", "IR_RIGHT", EV_MOTION, 0, 0},
    [C_CL_UP] = {"C↑", "CLASSIC_UP", EV_BUTTON, 0, 0},
    [C_CL_DOWN] = {"C↓", "CLASSIC_DOWN", EV_BUTTON, 0, 0},
    [C_CL_LEFT] = {"C←", "CLASSIC_LEFT", EV_BUTTON, 0, 0},
    [C_CL_RIGHT] = {"C→", "CLASSIC_RIGHT", EV_BUTTON, 0, 0},
    [C_CL_A] = {"CA", "CLASSIC_A", EV_BUTTON, 0, 0},
    [C_CL_B] = {"CB", "CLASSIC_B", EV_BUTTON, 0, 0},
    [C_CL_X] = {"CX", "CLASSIC_X", EV_BUTTON, 0, 0},
    [C_CL_Y] = {"CY", "CLASSIC_Y", EV_BUTTON, 0, 0},
    [C_CL_L] = {"CL", "CLASSIC_L", EV_BUTTON, 0, 0},
    [C_CL_R] = {"CR", "CLASSIC_R", EV_BUTTON, 0, 0},
    [C_CL_ZL] = {"ZL", "CLASSIC_ZL", EV_BUTTON, 0, 0},
    [C_CL_ZR] = {"ZR", "CLASSIC_ZR", EV_BUTTON, 0, 0},
    [C_CL_MINUS] = {"C−", "CLASSIC_MINUS", EV_BUTTON, 0, 0},
    [C_CL_PLUS] = {"C+", "CLASSIC_PLUS", EV_BUTTON, 0, 0},
};

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static int term_rows(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row) return ws.ws_row;
    return 24;
}

static int term_cols(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col) return ws.ws_col;
    return 80;
}

static void term_restore(void) {
    write(STDOUT_FILENO, ANSI_RESET DISABLE_KITTY_KBD "\033[?1049l\033[?25h", sizeof(ANSI_RESET DISABLE_KITTY_KBD "\033[?1049l\033[?25h") - 1);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void term_raw(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(1);
    }
    atexit(term_restore);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(1);
    }
    write(STDOUT_FILENO, ENABLE_KITTY_KBD "\033[?1049h\033[?25l", sizeof(ENABLE_KITTY_KBD "\033[?1049h\033[?25l") - 1);
}

static void sig_handler(int sig) {
    (void)sig;
    running = 0;
}

static void sig_winch(int sig) {
    (void)sig;
    need_redraw = 1;
}

static void appendf(char *buf, int *len, size_t size, const char *fmt, ...) {
    if (*len >= (int)size - 1) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + *len, size - *len, fmt, ap);
    va_end(ap);
    if (n > 0) {
        *len += n;
        if (*len >= (int)size) *len = (int)size - 1;
    }
}

static void repeat_text(char *buf, int *len, size_t size, const char *s, int n) {
    for (int i = 0; i < n; i++) appendf(buf, len, size, "%s", s);
}

static void send_raw(const char *msg) {
    if (sockfd < 0) return;
    ssize_t ret = sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&sock_addr, sizeof(sock_addr));
    if (ret < 0) {
        snprintf(last_event, sizeof(last_event), "send failed: %s", strerror(errno));
    } else {
        snprintf(last_event, sizeof(last_event), "%s", msg);
        event_count++;
        connected_hint = 1;
    }
}

static void send_control(int idx, int down) {
    char msg[128];
    const char *type = controls[idx].kind == EV_BUTTON ? "button" : "analog_motion";
    snprintf(msg, sizeof(msg), "%s %d %s", type, down ? 1 : 0, controls[idx].name);
    send_raw(msg);
}

static void release_control(int idx) {
    if (!controls[idx].active) return;
    controls[idx].active = 0;
    controls[idx].release_at_ms = 0;
    send_control(idx, 0);
}

static void press_control(int idx, int hold_ms) {
    long long deadline = now_ms() + hold_ms;
    if (!controls[idx].active) {
        controls[idx].active = 1;
        send_control(idx, 1);
    }
    controls[idx].release_at_ms = hold_ms > 0 ? deadline : 0;
}

static void toggle_control(int idx) {
    if (controls[idx].active)
        release_control(idx);
    else
        press_control(idx, 0);
}

static void release_all(void) {
    for (int i = 0; i < NCONTROLS; i++) release_control(i);
}

static void expire_controls(void) {
    long long t = now_ms();
    for (int i = 0; i < NCONTROLS; i++) {
        if (controls[i].active && controls[i].release_at_ms > 0 && controls[i].release_at_ms <= t)
            release_control(i);
    }
}

static void set_classic(int enabled) {
    classic_mode = enabled;
    release_all();
    if (enabled) send_raw("hotplug 1 classic");
    else send_raw("hotplug 0 none");
}

static void draw_button(char *buf, int *len, size_t size, int row, int col, int idx) {
    const char *style = controls[idx].active ? MAIN_SELECTED : MAIN_BASE;
    appendf(buf, len, size, "\033[%d;%dH%s[%-5s]%s", row, col, style, controls[idx].label, MAIN_BASE);
}

static void draw(void) {
    int rows = term_rows();
    int cols = term_cols();
    int side = cols >= 90 ? 30 : 0;
    int main_col = side ? side + 2 : 1;
    int main_cols = cols - (side ? side + 1 : 0);
    if (main_cols < 1) main_cols = 1;

    char buf[65536];
    int len = 0;
    appendf(buf, &len, sizeof(buf), MAIN_BASE "\033[2J\033[H");

    if (side) {
        for (int r = 1; r <= rows; r++) {
            appendf(buf, &len, sizeof(buf), "\033[%d;1H%s%*s%s", r, SIDEBAR_BASE, side, "", MAIN_BASE);
        }
        appendf(buf, &len, sizeof(buf), "\033[1;1H%s  WiiCtl%s", SIDEBAR_ACCENT_BOLD, SIDEBAR_BASE);
        appendf(buf, &len, sizeof(buf), "\033[2;1H%s", SIDEBAR_BORDER);
        repeat_text(buf, &len, sizeof(buf), SEP_H, side);
        const char *lines[] = {
            "tap key    toggle hold",
            "a/s/d/w    up/left/down/right",
            "j / 2      A",
            "k / bs     B",
            "1 / space  one / two",
            "- / =      minus / plus",
            "h          home",
            "shift/y    Y (classic)",
            "arrows     pointer drift",
            "m          classic toggle",
            "r          release all",
            "q          quit TUI",
            NULL
        };
        for (int i = 0; lines[i]; i++)
            appendf(buf, &len, sizeof(buf), "\033[%d;2H%s%-*.*s%s", i + 4, SIDEBAR_DIM, side - 2, side - 2, lines[i], SIDEBAR_BASE);
        for (int r = 1; r <= rows; r++)
            appendf(buf, &len, sizeof(buf), "\033[%d;%dH%s%s%s", r, side + 1, r == 2 ? SIDEBAR_BORDER : MAIN_BORDER_DIM, r == 2 ? SEP_CROSS : SEP_V, MAIN_BASE);
    }

    appendf(buf, &len, sizeof(buf), "\033[1;%dH%s  Wii U Controller%s", main_col, MAIN_ACCENT_BOLD, MAIN_BASE);
    appendf(buf, &len, sizeof(buf), " | mode: %s%s%s", classic_mode ? MAIN_WARN : MAIN_ACCENT, classic_mode ? "classic" : "wiimote", MAIN_BASE);
    appendf(buf, &len, sizeof(buf), " | socket: %s", socket_path);
    appendf(buf, &len, sizeof(buf), "\033[2;%dH%s", main_col, MAIN_BORDER);
    repeat_text(buf, &len, sizeof(buf), SEP_H, main_cols);

    int r = 4;
    appendf(buf, &len, sizeof(buf), "\033[%d;%dH%sD-pad%s", r, main_col, MAIN_ACCENT_BOLD, MAIN_BASE);
    draw_button(buf, &len, sizeof(buf), r + 1, main_col + 10, classic_mode ? C_CL_UP : C_UP);
    draw_button(buf, &len, sizeof(buf), r + 2, main_col + 2, classic_mode ? C_CL_LEFT : C_LEFT);
    draw_button(buf, &len, sizeof(buf), r + 2, main_col + 18, classic_mode ? C_CL_RIGHT : C_RIGHT);
    draw_button(buf, &len, sizeof(buf), r + 3, main_col + 10, classic_mode ? C_CL_DOWN : C_DOWN);
    appendf(buf, &len, sizeof(buf), "\033[%d;%dH%s      a\n\033[%d;%dH   s     w\n\033[%d;%dH      d%s", r + 5, main_col, MAIN_DIM, r + 6, main_col, r + 7, main_col, MAIN_BASE);

    int bcol = main_col + 38;
    appendf(buf, &len, sizeof(buf), "\033[%d;%dH%sButtons%s", r, bcol, MAIN_ACCENT_BOLD, MAIN_BASE);
    draw_button(buf, &len, sizeof(buf), r + 1, bcol, classic_mode ? C_CL_A : C_A);
    draw_button(buf, &len, sizeof(buf), r + 1, bcol + 12, classic_mode ? C_CL_B : C_B);
    draw_button(buf, &len, sizeof(buf), r + 2, bcol, classic_mode ? C_CL_X : C_ONE);
    draw_button(buf, &len, sizeof(buf), r + 2, bcol + 12, classic_mode ? C_CL_Y : C_TWO);
    draw_button(buf, &len, sizeof(buf), r + 3, bcol, classic_mode ? C_CL_MINUS : C_MINUS);
    draw_button(buf, &len, sizeof(buf), r + 3, bcol + 12, classic_mode ? C_CL_PLUS : C_PLUS);
    draw_button(buf, &len, sizeof(buf), r + 4, bcol, C_HOME);
    if (classic_mode) {
        draw_button(buf, &len, sizeof(buf), r + 5, bcol, C_CL_L);
        draw_button(buf, &len, sizeof(buf), r + 5, bcol + 12, C_CL_R);
        draw_button(buf, &len, sizeof(buf), r + 6, bcol, C_CL_ZL);
        draw_button(buf, &len, sizeof(buf), r + 6, bcol + 12, C_CL_ZR);
    }

    int prow = r + 10;
    appendf(buf, &len, sizeof(buf), "\033[%d;%dH%sPointer drift%s", prow, main_col, MAIN_ACCENT_BOLD, MAIN_BASE);
    draw_button(buf, &len, sizeof(buf), prow + 1, main_col + 10, C_IR_UP);
    draw_button(buf, &len, sizeof(buf), prow + 2, main_col + 2, C_IR_LEFT);
    draw_button(buf, &len, sizeof(buf), prow + 2, main_col + 18, C_IR_RIGHT);
    draw_button(buf, &len, sizeof(buf), prow + 3, main_col + 10, C_IR_DOWN);
    appendf(buf, &len, sizeof(buf), "\033[%d;%dH%sTap keys to toggle held/released. Multiple held inputs are supported. r releases all.%s", prow + 5, main_col, MAIN_DIM, MAIN_BASE);

    const char *status_style = connected_hint ? MAIN_ACCENT : MAIN_WARN;
    appendf(buf, &len, sizeof(buf), "\033[%d;%dH%s%-*.*s%s", rows - 1, main_col, status_style, main_cols, main_cols, last_event, MAIN_BASE);
    const char *help = classic_mode ? "toggle-hold: a/s/d/w dpad | j/k A/B | u or shift=Y | r release all | q quit" :
                                      "toggle-hold: a/s/d/w dpad | j or 2=A | space=2 | shift/y=Y | r release all | q quit";
    appendf(buf, &len, sizeof(buf), "\033[%d;%dH%s%-*.*s%s", rows, main_col, MAIN_DIM, main_cols, main_cols, help, MAIN_BASE);

    write(STDOUT_FILENO, buf, len);
}

static int parse_csi_u(unsigned char *buf, int n) {
    if (n < 4 || buf[0] != 0x1b || buf[1] != '[' || buf[n - 1] != 'u')
        return 0;

    char tmp[64];
    int m = n < (int)sizeof(tmp) - 1 ? n : (int)sizeof(tmp) - 1;
    memcpy(tmp, buf, m);
    tmp[m] = '\0';

    char *p = tmp + 2;
    long key = strtol(p, &p, 10);
    int event_type = 1;
    char *colon = strchr(tmp, ':');
    if (colon)
        event_type = (int)strtol(colon + 1, NULL, 10);

    if (event_type == 3)
        return 1;

    if (key == 57441 || key == 57447 || key == 57453 || key == 57454) {
        toggle_control(C_CL_Y);
        return 1;
    }

    return 0;
}

static int parse_key(unsigned char *buf, int n) {
    if (n <= 0) return 0;
    if (parse_csi_u(buf, n)) return 1;
    if (n >= 3 && buf[0] == 0x1b && buf[1] == '[') {
        switch (buf[2]) {
            case 'A': toggle_control(C_IR_UP); return 1;
            case 'B': toggle_control(C_IR_DOWN); return 1;
            case 'C': toggle_control(C_IR_RIGHT); return 1;
            case 'D': toggle_control(C_IR_LEFT); return 1;
        }
    }
    char c = (char)buf[0];
    switch (c) {
        case 'q': running = 0; return 1;
        case 'r': release_all(); snprintf(last_event, sizeof(last_event), "released all"); return 1;
        case 'm': set_classic(!classic_mode); return 1;
        case 'a': case 'A': toggle_control(classic_mode ? C_CL_UP : C_UP); return 1;
        case 'd': case 'D': toggle_control(classic_mode ? C_CL_DOWN : C_DOWN); return 1;
        case 's': case 'S': toggle_control(classic_mode ? C_CL_LEFT : C_LEFT); return 1;
        case 'w': case 'W': toggle_control(classic_mode ? C_CL_RIGHT : C_RIGHT); return 1;
        case 'j': case 'J': toggle_control(classic_mode ? C_CL_A : C_A); return 1;
        case 'k': case 'K': case 127: toggle_control(classic_mode ? C_CL_B : C_B); return 1;
        case 'u': case 'U': toggle_control(classic_mode ? C_CL_X : C_ONE); return 1;
        case 'i': case 'I': toggle_control(classic_mode ? C_CL_Y : C_TWO); return 1;
        case 'y': case 'Y': toggle_control(C_CL_Y); return 1;
        case '1': toggle_control(C_ONE); return 1;
        case '2': toggle_control(classic_mode ? C_CL_A : C_A); return 1;
        case ' ': toggle_control(C_TWO); return 1;
        case '-': toggle_control(classic_mode ? C_CL_MINUS : C_MINUS); return 1;
        case '=': case '+': toggle_control(classic_mode ? C_CL_PLUS : C_PLUS); return 1;
        case 'h': case 'H': toggle_control(C_HOME); return 1;
        case 'e': case 'E': if (classic_mode) { toggle_control(C_CL_R); return 1; } break;
        case 'o': case 'O': if (classic_mode) { toggle_control(C_CL_L); return 1; } break;
        case 'z': case 'Z': if (classic_mode) { toggle_control(C_CL_ZL); return 1; } break;
        case 'x': case 'X': if (classic_mode) { toggle_control(C_CL_ZR); return 1; } break;
    }
    return 0;
}

static void usage(const char *argv0) {
    fprintf(stderr, "usage: %s [socket-path]\n", argv0);
}

int main(int argc, char **argv) {
    const char *home = getenv("HOME");
    const char *default_path = "/tmp/wmemu.sock";
    static char home_path[108];
    if (home) {
        snprintf(home_path, sizeof(home_path), "%s/Workspace/research/wiimote/logs/wmemu.sock", home);
        default_path = home_path;
    }
    if (argc > 2) {
        usage(argv[0]);
        return 2;
    }
    snprintf(socket_path, sizeof(socket_path), "%s", argc == 2 ? argv[1] : default_path);

    sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sun_family = AF_UNIX;
    strncpy(sock_addr.sun_path, socket_path, sizeof(sock_addr.sun_path) - 1);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGWINCH, sig_winch);
    signal(SIGPIPE, SIG_IGN);

    term_raw();
    snprintf(last_event, sizeof(last_event), "ready: waiting for input; socket %s", socket_path);
    draw();

    struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
    while (running) {
        int ready = poll(&pfd, 1, 25);
        if (ready > 0 && (pfd.revents & POLLIN)) {
            unsigned char kb[32];
            int n = read(STDIN_FILENO, kb, sizeof(kb));
            if (n > 0) parse_key(kb, n);
        }
        expire_controls();
        if (need_redraw) {
            need_redraw = 0;
            draw();
        } else {
            draw();
        }
    }

    release_all();
    close(sockfd);
    return 0;
}
