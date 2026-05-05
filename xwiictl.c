// SPDX-License-Identifier: GPL-2.0-or-later
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define WIN_W 900
#define WIN_H 560
#define MAX_KEYCODE 256

static const unsigned long COL_BG = 0x00050f;
static const unsigned long COL_PANEL = 0x030814;
static const unsigned long COL_TEXT = 0xffffff;
static const unsigned long COL_DIM = 0x8a8f98;
static const unsigned long COL_ACCENT = 0x1d9bf0;
static const unsigned long COL_HELD = 0x1d9bf0;
static const unsigned long COL_WARN = 0xfec66b;
static const unsigned long COL_BAD = 0xfe6b6b;
static const unsigned long COL_BORDER = 0x4e4f4f;

typedef enum { EV_BUTTON, EV_MOTION } EventKind;

typedef struct {
    const char *label;
    const char *name;
    EventKind kind;
    int held_count;
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
    [C_UP] = {"D↑", "WIIMOTE_UP", EV_BUTTON, 0},
    [C_DOWN] = {"D↓", "WIIMOTE_DOWN", EV_BUTTON, 0},
    [C_LEFT] = {"D←", "WIIMOTE_LEFT", EV_BUTTON, 0},
    [C_RIGHT] = {"D→", "WIIMOTE_RIGHT", EV_BUTTON, 0},
    [C_A] = {"A", "WIIMOTE_A", EV_BUTTON, 0},
    [C_B] = {"B", "WIIMOTE_B", EV_BUTTON, 0},
    [C_ONE] = {"1", "WIIMOTE_1", EV_BUTTON, 0},
    [C_TWO] = {"2", "WIIMOTE_2", EV_BUTTON, 0},
    [C_MINUS] = {"−", "WIIMOTE_MINUS", EV_BUTTON, 0},
    [C_PLUS] = {"+", "WIIMOTE_PLUS", EV_BUTTON, 0},
    [C_HOME] = {"Home", "HOME", EV_BUTTON, 0},
    [C_IR_UP] = {"IR↑", "IR_UP", EV_MOTION, 0},
    [C_IR_DOWN] = {"IR↓", "IR_DOWN", EV_MOTION, 0},
    [C_IR_LEFT] = {"IR←", "IR_LEFT", EV_MOTION, 0},
    [C_IR_RIGHT] = {"IR→", "IR_RIGHT", EV_MOTION, 0},
    [C_CL_UP] = {"C↑", "CLASSIC_UP", EV_BUTTON, 0},
    [C_CL_DOWN] = {"C↓", "CLASSIC_DOWN", EV_BUTTON, 0},
    [C_CL_LEFT] = {"C←", "CLASSIC_LEFT", EV_BUTTON, 0},
    [C_CL_RIGHT] = {"C→", "CLASSIC_RIGHT", EV_BUTTON, 0},
    [C_CL_A] = {"CA", "CLASSIC_A", EV_BUTTON, 0},
    [C_CL_B] = {"CB", "CLASSIC_B", EV_BUTTON, 0},
    [C_CL_X] = {"CX", "CLASSIC_X", EV_BUTTON, 0},
    [C_CL_Y] = {"CY", "CLASSIC_Y", EV_BUTTON, 0},
    [C_CL_L] = {"CL", "CLASSIC_L", EV_BUTTON, 0},
    [C_CL_R] = {"CR", "CLASSIC_R", EV_BUTTON, 0},
    [C_CL_ZL] = {"ZL", "CLASSIC_ZL", EV_BUTTON, 0},
    [C_CL_ZR] = {"ZR", "CLASSIC_ZR", EV_BUTTON, 0},
    [C_CL_MINUS] = {"C−", "CLASSIC_MINUS", EV_BUTTON, 0},
    [C_CL_PLUS] = {"C+", "CLASSIC_PLUS", EV_BUTTON, 0},
};

static Display *dpy;
static int screen;
static Window win;
static GC gc;
static XFontStruct *font;
static Atom wm_delete_window;
static int sockfd = -1;
static struct sockaddr_un sock_addr;
static char socket_path[108];
static int classic_mode = 0;
static int focused = 0;
static char last_event[160] = "ready";
static int event_count = 0;
static int key_down[MAX_KEYCODE];
static int key_control[MAX_KEYCODE];
static int running = 1;
static KeyCode shift_l_code = 0;
static KeyCode shift_r_code = 0;

static unsigned long px(unsigned long rgb) {
    Colormap cmap = DefaultColormap(dpy, screen);
    XColor c;
    c.red = ((rgb >> 16) & 0xff) * 257;
    c.green = ((rgb >> 8) & 0xff) * 257;
    c.blue = (rgb & 0xff) * 257;
    c.flags = DoRed | DoGreen | DoBlue;
    if (!XAllocColor(dpy, cmap, &c)) return BlackPixel(dpy, screen);
    return c.pixel;
}

static void set_fg(unsigned long rgb) {
    XSetForeground(dpy, gc, px(rgb));
}

static void fill_rect(int x, int y, int w, int h, unsigned long rgb) {
    set_fg(rgb);
    XFillRectangle(dpy, win, gc, x, y, (unsigned)w, (unsigned)h);
}

static void draw_rect(int x, int y, int w, int h, unsigned long rgb) {
    set_fg(rgb);
    XDrawRectangle(dpy, win, gc, x, y, (unsigned)w, (unsigned)h);
}

static void draw_text(int x, int y, unsigned long rgb, const char *s) {
    set_fg(rgb);
    XDrawString(dpy, win, gc, x, y, s, (int)strlen(s));
}

static void send_raw(const char *msg) {
    if (sockfd < 0) return;
    ssize_t ret = sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&sock_addr, sizeof(sock_addr));
    if (ret < 0) {
        snprintf(last_event, sizeof(last_event), "send failed: %s", strerror(errno));
        fprintf(stderr, "%s\n", last_event);
        fflush(stderr);
    } else {
        snprintf(last_event, sizeof(last_event), "%s", msg);
        fprintf(stderr, "%s\n", msg);
        fflush(stderr);
        event_count++;
    }
}

static void send_control(int idx, int down) {
    char msg[128];
    const char *type = controls[idx].kind == EV_BUTTON ? "button" : "analog_motion";
    snprintf(msg, sizeof(msg), "%s %d %s", type, down ? 1 : 0, controls[idx].name);
    send_raw(msg);
}

static void hold_control(int idx) {
    if (idx < 0 || idx >= NCONTROLS) return;
    if (controls[idx].held_count++ == 0) send_control(idx, 1);
}

static void release_control(int idx) {
    if (idx < 0 || idx >= NCONTROLS) return;
    if (controls[idx].held_count <= 0) return;
    controls[idx].held_count--;
    if (controls[idx].held_count == 0) send_control(idx, 0);
}

static void release_all(void) {
    for (int i = 0; i < NCONTROLS; i++) {
        if (controls[i].held_count > 0) {
            controls[i].held_count = 1;
            release_control(i);
        }
    }
    memset(key_down, 0, sizeof(key_down));
    for (int i = 0; i < MAX_KEYCODE; i++) key_control[i] = -1;
    snprintf(last_event, sizeof(last_event), "released all");
}

static void set_classic(int enabled) {
    if (classic_mode == enabled) return;
    release_all();
    classic_mode = enabled;
    send_raw(enabled ? "hotplug 1 classic" : "hotplug 0 none");
}

static int map_keysym(KeySym ks) {
    switch (ks) {
        /* Current requested directional layout. */
        case XK_a: case XK_A: return classic_mode ? C_CL_UP : C_UP;
        case XK_s: case XK_S: return classic_mode ? C_CL_LEFT : C_LEFT;
        case XK_d: case XK_D: return classic_mode ? C_CL_DOWN : C_DOWN;
        case XK_w: case XK_W: return classic_mode ? C_CL_RIGHT : C_RIGHT;

        /* Swapped space and 2: 2=A, Space=Wiimote 2. */
        case XK_2: case XK_at: return classic_mode ? C_CL_A : C_A;
        case XK_space: return C_TWO;
        case XK_j: case XK_J: return classic_mode ? C_CL_B : C_B;
        case XK_k: case XK_K: case XK_BackSpace: return classic_mode ? C_CL_A : C_A;
        case XK_1: case XK_exclam: return C_ONE;
        case XK_minus: case XK_underscore: return classic_mode ? C_CL_MINUS : C_MINUS;
        case XK_equal: case XK_plus: return classic_mode ? C_CL_PLUS : C_PLUS;
        case XK_h: case XK_H: return C_HOME;

        /* Super Mario 3D World with a sideways Wii Remote uses Wiimote 1
         * as run/dash. Classic/GamePad Y only exists in classic mode.
         */
        case XK_Shift_L: case XK_Shift_R: return classic_mode ? C_CL_Y : C_ONE;
        case XK_y: case XK_Y: return classic_mode ? C_CL_Y : C_ONE;
        case XK_u: case XK_U: return classic_mode ? C_CL_X : C_ONE;
        case XK_i: case XK_I: return classic_mode ? C_CL_Y : C_TWO;
        case XK_o: case XK_O: return classic_mode ? C_CL_L : -1;
        case XK_e: case XK_E: return classic_mode ? C_CL_R : -1;
        case XK_z: case XK_Z: return classic_mode ? C_CL_ZL : -1;
        case XK_x: case XK_X: return classic_mode ? C_CL_ZR : -1;

        /* Pointer drift / IR. */
        case XK_Up: return C_IR_UP;
        case XK_Down: return C_IR_DOWN;
        case XK_Left: return C_IR_LEFT;
        case XK_Right: return C_IR_RIGHT;
    }
    return -1;
}

static int keycode_is_down(const char keys[32], KeyCode kc) {
    return (keys[kc / 8] & (1 << (kc % 8))) != 0;
}

static void sync_keycode_control(KeyCode kc, int ctrl, const char keys[32]) {
    if (kc == 0 || ctrl < 0) return;
    int down = keycode_is_down(keys, kc);
    if (down && !key_down[kc]) {
        key_down[kc] = 1;
        key_control[kc] = ctrl;
        hold_control(ctrl);
    } else if (!down && key_down[kc]) {
        int old = key_control[kc];
        key_down[kc] = 0;
        key_control[kc] = -1;
        release_control(old);
    }
}

static void sync_keyboard_state(void) {
    char keys[32];
    XQueryKeymap(dpy, keys);

#define SYNC_KEY(sym, ctrl) sync_keycode_control(XKeysymToKeycode(dpy, (sym)), (ctrl), keys)
    SYNC_KEY(XK_a, classic_mode ? C_CL_UP : C_UP);
    SYNC_KEY(XK_s, classic_mode ? C_CL_LEFT : C_LEFT);
    SYNC_KEY(XK_d, classic_mode ? C_CL_DOWN : C_DOWN);
    SYNC_KEY(XK_w, classic_mode ? C_CL_RIGHT : C_RIGHT);
    SYNC_KEY(XK_j, classic_mode ? C_CL_B : C_B);
    SYNC_KEY(XK_2, classic_mode ? C_CL_A : C_A);
    SYNC_KEY(XK_space, C_TWO);
    SYNC_KEY(XK_k, classic_mode ? C_CL_A : C_A);
    SYNC_KEY(XK_BackSpace, classic_mode ? C_CL_A : C_A);
    SYNC_KEY(XK_1, C_ONE);
    SYNC_KEY(XK_minus, classic_mode ? C_CL_MINUS : C_MINUS);
    SYNC_KEY(XK_equal, classic_mode ? C_CL_PLUS : C_PLUS);
    SYNC_KEY(XK_h, C_HOME);
    SYNC_KEY(XK_y, classic_mode ? C_CL_Y : C_ONE);
    SYNC_KEY(XK_Shift_L, classic_mode ? C_CL_Y : C_ONE);
    SYNC_KEY(XK_Shift_R, classic_mode ? C_CL_Y : C_ONE);
    SYNC_KEY(XK_u, classic_mode ? C_CL_X : C_ONE);
    SYNC_KEY(XK_i, classic_mode ? C_CL_Y : C_TWO);
    if (classic_mode) {
        SYNC_KEY(XK_o, C_CL_L);
        SYNC_KEY(XK_e, C_CL_R);
        SYNC_KEY(XK_z, C_CL_ZL);
        SYNC_KEY(XK_x, C_CL_ZR);
    }
    SYNC_KEY(XK_Up, C_IR_UP);
    SYNC_KEY(XK_Down, C_IR_DOWN);
    SYNC_KEY(XK_Left, C_IR_LEFT);
    SYNC_KEY(XK_Right, C_IR_RIGHT);
#undef SYNC_KEY
}

static void draw_button(int x, int y, int w, int h, int idx, const char *key_hint) {
    int held = controls[idx].held_count > 0;
    fill_rect(x, y, w, h, held ? COL_HELD : COL_BG);
    draw_rect(x, y, w, h, held ? COL_ACCENT : COL_BORDER);
    char line[64];
    snprintf(line, sizeof(line), "%s", controls[idx].label);
    draw_text(x + 10, y + 24, held ? COL_TEXT : COL_TEXT, line);
    draw_text(x + 10, y + 45, held ? COL_TEXT : COL_DIM, key_hint);
}

static void redraw(void) {
    fill_rect(0, 0, WIN_W, WIN_H, COL_BG);
    fill_rect(0, 0, 270, WIN_H, COL_PANEL);
    draw_text(24, 34, COL_ACCENT, "WiiCtl X11");
    draw_text(24, 64, COL_DIM, "Real key press/release tracking");
    draw_text(24, 100, COL_TEXT, "Focus this window to play.");
    draw_text(24, 126, focused ? COL_ACCENT : COL_WARN, focused ? "focused" : "not focused");
    draw_text(24, 166, COL_TEXT, "a/s/d/w  = up/left/down/right");
    draw_text(24, 190, COL_TEXT, "2        = A");
    draw_text(24, 214, COL_TEXT, "space    = Wii 2");
    draw_text(24, 238, COL_TEXT, "j        = B / ground pound");
    draw_text(24, 262, COL_TEXT, "1        = Wii 1");
    draw_text(24, 286, COL_TEXT, classic_mode ? "shift/y  = Classic Y" : "shift/y  = Wii 1/run");
    draw_text(24, 310, COL_TEXT, "arrows   = pointer drift");
    draw_text(24, 334, COL_TEXT, "m        = toggle classic");
    draw_text(24, 358, COL_TEXT, "r        = release all");
    draw_text(24, 382, COL_TEXT, "q/esc    = quit");
    draw_text(24, 430, classic_mode ? COL_WARN : COL_ACCENT, classic_mode ? "mode: classic" : "mode: wiimote");

    char counts[128];
    snprintf(counts, sizeof(counts), "events sent: %d", event_count);
    draw_text(24, 470, COL_DIM, counts);

    draw_text(300, 36, COL_ACCENT, "D-pad");
    int up = classic_mode ? C_CL_UP : C_UP;
    int down = classic_mode ? C_CL_DOWN : C_DOWN;
    int left = classic_mode ? C_CL_LEFT : C_LEFT;
    int right = classic_mode ? C_CL_RIGHT : C_RIGHT;
    draw_button(390, 64, 90, 58, up, "a");
    draw_button(300, 130, 90, 58, left, "s");
    draw_button(480, 130, 90, 58, right, "w");
    draw_button(390, 196, 90, 58, down, "d");

    draw_text(620, 36, COL_ACCENT, "Buttons");
    draw_button(620, 64, 90, 58, classic_mode ? C_CL_A : C_A, "2 / k");
    draw_button(730, 64, 90, 58, classic_mode ? C_CL_B : C_B, "j");
    draw_button(620, 130, 90, 58, classic_mode ? C_CL_X : C_ONE, classic_mode ? "u" : "1/u");
    draw_button(730, 130, 90, 58, classic_mode ? C_CL_Y : C_TWO, classic_mode ? "shift/y" : "space/i");
    draw_button(620, 196, 90, 58, classic_mode ? C_CL_MINUS : C_MINUS, "-");
    draw_button(730, 196, 90, 58, classic_mode ? C_CL_PLUS : C_PLUS, "+/=");
    draw_button(620, 262, 90, 58, C_HOME, "h");

    if (classic_mode) {
        draw_text(300, 295, COL_ACCENT, "Classic shoulder buttons");
        draw_button(300, 320, 90, 58, C_CL_L, "o");
        draw_button(410, 320, 90, 58, C_CL_R, "e");
        draw_button(520, 320, 90, 58, C_CL_ZL, "z");
        draw_button(630, 320, 90, 58, C_CL_ZR, "x");
    }

    draw_text(300, 420, COL_ACCENT, "Pointer drift");
    draw_button(390, 446, 90, 58, C_IR_UP, "↑");
    draw_button(300, 496, 90, 58, C_IR_LEFT, "←");
    draw_button(480, 496, 90, 58, C_IR_RIGHT, "→");
    draw_button(390, 496, 90, 58, C_IR_DOWN, "↓");

    fill_rect(270, WIN_H - 28, WIN_W - 270, 28, COL_BG);
    draw_text(288, WIN_H - 9, COL_DIM, last_event);
    XFlush(dpy);
}

static int is_repeat_release(XEvent *ev) {
    if (XEventsQueued(dpy, QueuedAfterReading) <= 0) return 0;
    XEvent next;
    XPeekEvent(dpy, &next);
    return next.type == KeyPress && next.xkey.keycode == ev->xkey.keycode && next.xkey.time == ev->xkey.time;
}

static void handle_key_press(XKeyEvent *ke) {
    KeyCode kc = ke->keycode;
    if (kc == shift_l_code || kc == shift_r_code) {
        if (!key_down[kc]) {
            key_down[kc] = 1;
            key_control[kc] = classic_mode ? C_CL_Y : C_ONE;
            hold_control(key_control[kc]);
        }
        return;
    }
    KeySym ks = XLookupKeysym(ke, 0);
    if (ks == XK_q || ks == XK_Q || ks == XK_Escape) {
        running = 0;
        return;
    }
    if (ks == XK_r || ks == XK_R) {
        release_all();
        return;
    }
    if (ks == XK_m || ks == XK_M) {
        set_classic(!classic_mode);
        return;
    }
    if (kc >= MAX_KEYCODE) return;
    if (key_down[kc]) return;
    int ctrl = map_keysym(ks);
    if (ctrl < 0) return;
    key_down[kc] = 1;
    key_control[kc] = ctrl;
    hold_control(ctrl);
}

static void handle_key_release(XEvent *ev) {
    if (is_repeat_release(ev)) return;
    KeyCode kc = ev->xkey.keycode;
    if (kc >= MAX_KEYCODE) return;
    if (!key_down[kc]) return;
    int ctrl = key_control[kc];
    key_down[kc] = 0;
    key_control[kc] = -1;
    release_control(ctrl);
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

    for (int i = 0; i < MAX_KEYCODE; i++) key_control[i] = -1;

    sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sun_family = AF_UNIX;
    strncpy(sock_addr.sun_path, socket_path, sizeof(sock_addr.sun_path) - 1);

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Could not open X display\n");
        return 1;
    }
    screen = DefaultScreen(dpy);

    win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 80, 80, WIN_W, WIN_H, 1,
                              px(COL_BORDER), px(COL_BG));
    XStoreName(dpy, win, "WiiCtl X11 Input Master");
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | KeyReleaseMask |
                          FocusChangeMask | StructureNotifyMask);
    wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete_window, 1);

    gc = XCreateGC(dpy, win, 0, NULL);
    font = XLoadQueryFont(dpy, "9x15");
    if (!font) font = XLoadQueryFont(dpy, "fixed");
    if (font) XSetFont(dpy, gc, font->fid);

    shift_l_code = XKeysymToKeycode(dpy, XK_Shift_L);
    shift_r_code = XKeysymToKeycode(dpy, XK_Shift_R);

    int xkb_event, xkb_error, xkb_major = XkbMajorVersion, xkb_minor = XkbMinorVersion;
    if (XkbLibraryVersion(&xkb_major, &xkb_minor) &&
        XkbQueryExtension(dpy, NULL, &xkb_event, &xkb_error, &xkb_major, &xkb_minor)) {
        Bool supported = False;
        XkbSetDetectableAutoRepeat(dpy, True, &supported);
    }

    XMapRaised(dpy, win);

    while (running) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            switch (ev.type) {
                case Expose:
                    if (ev.xexpose.count == 0) redraw();
                    break;
                case ConfigureNotify:
                    redraw();
                    break;
                case FocusIn:
                    focused = 1;
                    redraw();
                    break;
                case FocusOut:
                    focused = 0;
                    release_all();
                    redraw();
                    break;
                case KeyPress:
                    handle_key_press(&ev.xkey);
                    redraw();
                    break;
                case KeyRelease:
                    handle_key_release(&ev);
                    redraw();
                    break;
                case ClientMessage:
                    if ((Atom)ev.xclient.data.l[0] == wm_delete_window) running = 0;
                    break;
            }
        }
        if (focused)
            sync_keyboard_state();
        redraw();
        usleep(16000);
    }

    release_all();
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    close(sockfd);
    return 0;
}
