/*
 * widget.c - implementacion de la base comun (Win32 + GDI, sin dependencias).
 *
 * Decisiones de consumo:
 *  - Ventana "layered" con UpdateLayeredWindow: transparencia por pixel real,
 *    sin DWM extra ni recuadro. Pixel con alfa 0 = clic pasa a lo de atras.
 *  - El bucle espera con MsgWaitForMultipleObjects: si fps == 0 o la ventana
 *    esta oculta, el proceso duerme hasta que llegue un mensaje (CPU 0%).
 *  - Con fps > 0 se usa un waitable timer de alta resolucion, sin
 *    timeBeginPeriod (que subiria el consumo de todo el sistema).
 *  - Solo se redibuja cuando algo cambia (dirty flag).
 */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <math.h>
#include "widget.h"

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

#define CLASS_NAME L"fidgets_widget"
#define HK_HIDEALL 1
#define HK_QUITALL 2
#define WM_SHOWME (WM_APP + 1)

enum { ID_TOP = 100, ID_DIM, ID_HIDEALL, ID_CLOSE, ID_QUITALL, ID_OPA = 200, ID_CUSTOM = 1000 };

static const WidgetDesc *D;
static HWND hwnd;
static HDC memdc, txtdc;
static uint32_t *px, *txtpx;
static int PW, PH, S = 1;           /* tamano fisico y escala */
static int fps, dirty, hidden, hover, own_hotkeys;
static int topmost = 1, opacity = 100, dim = 0;
static int close_by_user;           /* 1 si el usuario cerro este widget (clic central, menu) */
static HANDLE timer;
static int timer_period;            /* ms; 0 = apagado */
static double last_tick;
static wchar_t ini_path[MAX_PATH];
static UINT MSG_HIDEALL, MSG_QUITALL, MSG_REHOOK, MSG_USER;
static LARGE_INTEGER qpf;
static uint32_t rng_state = 2463534242u;

/* ------------------------------------------------------------------ util */

double now_seconds(void) {
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)qpf.QuadPart;
}

uint32_t rnd(void) {
    uint32_t x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return rng_state = x;
}

int rnd_range(int lo, int hi) { return lo + (int)(rnd() % (uint32_t)(hi - lo + 1)); }

/* ---------------------------------------------------------------- config */

int cfg_get_int(const wchar_t *key, int def) {
    return (int)GetPrivateProfileIntW(D->name, key, def, ini_path);
}

void cfg_set_int(const wchar_t *key, int value) { cfg_set_int_in(D->name, key, value); }

int cfg_get_int_in(const wchar_t *section, const wchar_t *key, int def) {
    return (int)GetPrivateProfileIntW(section, key, def, ini_path);
}

void cfg_set_int_in(const wchar_t *section, const wchar_t *key, int value) {
    wchar_t buf[16];
    wsprintfW(buf, L"%d", value);
    WritePrivateProfileStringW(section, key, buf, ini_path);
}

/* TEMPORAL (quitar antes de publicar): renombra una clave de la seccion del widget
 * si la nueva aun no existe. Devuelve 1 si la renombro. */
int cfg_rename(const wchar_t *old_key, const wchar_t *new_key) {
    wchar_t v[64];
    if (GetPrivateProfileStringW(D->name, new_key, L"", v, 64, ini_path) > 0) return 0;
    if (GetPrivateProfileStringW(D->name, old_key, L"", v, 64, ini_path) == 0) return 0;
    WritePrivateProfileStringW(D->name, new_key, v, ini_path);
    WritePrivateProfileStringW(D->name, old_key, NULL, ini_path);
    return 1;
}

/* Ajustes en %APPDATA%\fidgets\<nombre del exe>.ini, asi el programa puede estar en
 * cualquier carpeta (incluso una sin permiso de escritura). FIDGETS_INI=ruta lo
 * reemplaza (pruebas). */
#define CONFIG_DIR L"fidgets"

/* TEMPORAL (quitar antes de publicar): migracion desde el nombre provisional "bored".
 * Si la seccion del widget aun no existe, se copia la de
 * %APPDATA%\bored\<legacy_ini>.ini (seccion legacy_section). */
void cfg_migrate_section(const wchar_t *section, const wchar_t *legacy_ini, const wchar_t *legacy_section) {
    static wchar_t buf[8192];
    wchar_t appdata[MAX_PATH], old[MAX_PATH];
    if (!legacy_ini || !GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH)) return;
    if (GetPrivateProfileSectionW(section, buf, 8192, ini_path) > 0) return;
    wsprintfW(old, L"%s\\bored\\%s.ini", appdata, legacy_ini);
    DWORD n = GetPrivateProfileSectionW(legacy_section ? legacy_section : legacy_ini, buf, 8192, old);
    if (n > 0 && n < 8190) WritePrivateProfileSectionW(section, buf, ini_path);
}

static void find_ini(void) {
    if (GetEnvironmentVariableW(L"FIDGETS_INI", ini_path, MAX_PATH) > 0) return;
    wchar_t exe[MAX_PATH], appdata[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    wchar_t *base = exe, *dot = 0;
    for (wchar_t *p = exe; *p; p++) if (*p == L'\\' || *p == L'/') base = p + 1;
    for (wchar_t *p = base; *p; p++) if (*p == L'.') dot = p;
    if (!GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH)) {   /* sin APPDATA: junto al .exe */
        if (dot) lstrcpyW(dot, L".ini");
        lstrcpyW(ini_path, exe);
        return;
    }
    if (dot) *dot = 0;
    wsprintfW(ini_path, L"%s\\%s", appdata, CONFIG_DIR);
    CreateDirectoryW(ini_path, NULL);
    lstrcatW(ini_path, L"\\");
    lstrcatW(ini_path, base);
    lstrcatW(ini_path, L".ini");
    cfg_migrate_section(D->name, D->legacy_ini, D->legacy_section);
}

/* ---------------------------------------------------------------- tema */

static const Theme default_theme = {
    .text   = RGBA(240, 242, 248, 240),
    .sub    = RGBA(150, 156, 170, 215),
    .accent = RGBA(250, 190, 70, 240),
    .error  = RGBA(235, 80, 80, 240),
    .extra  = RGBA(160, 55, 60, 230),
};
const Theme *theme = &default_theme;

/* ------------------------------------------------------------------ gfx */

static inline unsigned div255(unsigned x) { return (x + 1 + (x >> 8)) >> 8; }

static inline uint32_t premul(Color c, unsigned cov) {
    unsigned a = div255((c >> 24) * cov);
    return (a << 24) | (div255(((c >> 16) & 255) * a) << 16) |
           (div255(((c >> 8) & 255) * a) << 8) | div255((c & 255) * a);
}

static inline void blend(uint32_t *d, uint32_t s) {
    unsigned sa = s >> 24;
    if (sa == 255) { *d = s; return; }
    if (sa == 0) return;
    unsigned inv = 255 - sa, v = *d;
    *d = ((sa + div255((v >> 24) * inv)) << 24) |
         ((((s >> 16) & 255) + div255(((v >> 16) & 255) * inv)) << 16) |
         ((((s >> 8) & 255) + div255(((v >> 8) & 255) * inv)) << 8) |
         ((s & 255) + div255((v & 255) * inv));
}

void gfx_clear(Color c) {
    uint32_t p = premul(c, 255);
    for (int i = 0, n = PW * PH; i < n; i++) px[i] = p;
}

/* Windows deja pasar el clic donde el alfa final es 0. El alfa final es
 * alfa_pixel * opacidad, asi que se usa el minimo que sigue siendo >= 1 con la
 * opacidad actual: a 25% son 5/255 de negro, imperceptible. */
void gfx_clear_hit(void) {
    unsigned k = (unsigned)opacity * 255 / 100;
    if (dim) k = k * 2 / 5;           /* "atenuar sin el mouse" baja aun mas el alfa */
    if (k < 1) k = 1;
    unsigned a = (255 + k - 1) / k;
    gfx_clear(RGBA(0, 0, 0, a));
}

void gfx_rect(int x, int y, int w, int h, Color c) {
    int x0 = x * S, y0 = y * S, x1 = (x + w) * S, y1 = (y + h) * S;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > PW) x1 = PW;
    if (y1 > PH) y1 = PH;
    uint32_t p = premul(c, 255);
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++) blend(&px[j * PW + i], p);
}

/* Rectangulo con esquinas redondeadas y borde suavizado (antialias). */
static void shape(float fx0, float fy0, float fx1, float fy1, float r, Color c) {
    int x0 = (int)fx0, y0 = (int)fy0, x1 = (int)(fx1 + 1), y1 = (int)(fy1 + 1);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > PW) x1 = PW;
    if (y1 > PH) y1 = PH;
    float ix0 = fx0 + r, iy0 = fy0 + r, ix1 = fx1 - r, iy1 = fy1 - r;
    for (int j = y0; j < y1; j++) {
        float py = j + 0.5f;
        float dy = py < iy0 ? iy0 - py : (py > iy1 ? py - iy1 : 0);
        for (int i = x0; i < x1; i++) {
            float pxf = i + 0.5f;
            float dx = pxf < ix0 ? ix0 - pxf : (pxf > ix1 ? pxf - ix1 : 0);
            float d2 = dx * dx + dy * dy, cov;
            if (d2 == 0) cov = 1;
            else {
                cov = r - sqrtf(d2) + 0.5f; /* solo en el borde; sqrtss nativo */
                if (cov <= 0) continue;
                if (cov > 1) cov = 1;
            }
            blend(&px[j * PW + i], premul(c, (unsigned)(cov * 255)));
        }
    }
}

void gfx_round_rect(int x, int y, int w, int h, int r, Color c) {
    shape((float)(x * S), (float)(y * S), (float)((x + w) * S), (float)((y + h) * S), (float)(r * S), c);
}

void gfx_circle(float cx, float cy, float r, Color c) {
    float s = (float)S;
    shape((cx - r) * s, (cy - r) * s, (cx + r) * s, (cy + r) * s, r * s, c);
}

/* Poligono convexo con antialias: cobertura = distancia al borde mas cercano. */
void gfx_poly(const float *xy, int n, Color c) {
    float p[16], nx[8], ny[8], nd[8];
    if (n < 3 || n > 8) return;
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f, area = 0;
    for (int i = 0; i < n; i++) {
        p[2 * i] = xy[2 * i] * S;
        p[2 * i + 1] = xy[2 * i + 1] * S;
        if (p[2 * i] < minx) minx = p[2 * i];
        if (p[2 * i] > maxx) maxx = p[2 * i];
        if (p[2 * i + 1] < miny) miny = p[2 * i + 1];
        if (p[2 * i + 1] > maxy) maxy = p[2 * i + 1];
    }
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        area += p[2 * i] * p[2 * j + 1] - p[2 * j] * p[2 * i + 1];
    }
    float sgn = area > 0 ? 1.0f : -1.0f;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        float ex = p[2 * j] - p[2 * i], ey = p[2 * j + 1] - p[2 * i + 1];
        float len = sqrtf(ex * ex + ey * ey);
        if (len == 0) len = 1;
        nx[i] = -ey / len * sgn;   /* normal hacia dentro */
        ny[i] = ex / len * sgn;
        nd[i] = nx[i] * p[2 * i] + ny[i] * p[2 * i + 1];
    }
    int x0 = (int)minx, y0 = (int)miny, x1 = (int)maxx + 1, y1 = (int)maxy + 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > PW) x1 = PW;
    if (y1 > PH) y1 = PH;
    for (int j = y0; j < y1; j++) {
        float fy = j + 0.5f;
        for (int i = x0; i < x1; i++) {
            float fx = i + 0.5f, d = 1e9f;
            for (int k = 0; k < n; k++) {
                float e = nx[k] * fx + ny[k] * fy - nd[k];
                if (e < d) d = e;
            }
            float cov = d + 0.5f;
            if (cov <= 0) continue;
            blend(&px[j * PW + i], premul(c, cov >= 1 ? 255 : (unsigned)(cov * 255)));
        }
    }
}

/* Texto: GDI dibuja en blanco sobre negro en un buffer auxiliar y el gris
 * resultante se usa como cobertura. Asi se conserva el canal alfa. */
static HFONT font_for(int size, int flags) {
    static HFONT cache[6];
    static int keys[6], next;
    int key = size * 2 + ((flags & TEXT_MONO) != 0);
    for (int i = 0; i < 6; i++)
        if (cache[i] && keys[i] == key) return cache[i];
    if (cache[next]) DeleteObject(cache[next]);
    int mono = (flags & TEXT_MONO) != 0;
    cache[next] = CreateFontW(-size * S, 0, 0, 0, mono ? FW_NORMAL : FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                              mono ? FIXED_PITCH : DEFAULT_PITCH, mono ? L"Consolas" : L"Segoe UI");
    keys[next] = key;
    HFONT f = cache[next];
    next = (next + 1) % 6;
    return f;
}

int gfx_text_width(const wchar_t *s, int n, int size, int flags) {
    if (n < 0) n = lstrlenW(s);
    if (!n) return 0;
    SelectObject(txtdc, font_for(size, flags));
    SIZE ext;
    GetTextExtentPoint32W(txtdc, s, n, &ext);
    return (ext.cx + S - 1) / S;
}

void gfx_text(int x, int y, const wchar_t *s, Color c, int size, int align) {
    int len = lstrlenW(s);
    if (!len) return;
    SelectObject(txtdc, font_for(size, align));
    SIZE ext;
    GetTextExtentPoint32W(txtdc, s, len, &ext);
    int tw = ext.cx < PW ? ext.cx : PW, th = ext.cy < PH ? ext.cy : PH;
    for (int j = 0; j < th; j++)
        for (int i = 0; i < tw; i++) txtpx[j * PW + i] = 0;
    TextOutW(txtdc, 0, 0, s, len);
    GdiFlush();
    int ox = x * S, oy = y * S, al = align & 3;
    if (al == ALIGN_CENTER) ox -= tw / 2;
    else if (al == ALIGN_RIGHT) ox -= tw;
    /* pasada 0: sombra negra desplazada 1 px (si se pidio); pasada 1: el texto */
    Color shadow = RGBA(0, 0, 0, (c >> 24) * 3 / 4);
    for (int pass = (align & TEXT_SHADOW) ? 0 : 1; pass < 2; pass++) {
        int sx = pass ? 0 : S, sy = pass ? 0 : S;
        Color col = pass ? c : shadow;
        for (int j = 0; j < th; j++) {
            int dy = oy + j + sy;
            if (dy < 0 || dy >= PH) continue;
            for (int i = 0; i < tw; i++) {
                int dx = ox + i + sx;
                if (dx < 0 || dx >= PW) continue;
                unsigned cov = (txtpx[j * PW + i] >> 8) & 255;
                if (cov) blend(&px[dy * PW + dx], premul(col, cov));
            }
        }
    }
}

/* -------------------------------------------------------------- ventana */

static void render(void) {
    dirty = 0;
    if (D->on_draw) D->on_draw();
    unsigned a = (unsigned)opacity * 255 / 100;
    if (dim && !hover) a = a * 2 / 5;
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)a, AC_SRC_ALPHA };
    POINT src = { 0, 0 };
    SIZE sz = { PW, PH };
    UpdateLayeredWindow(hwnd, NULL, NULL, &sz, memdc, &src, 0, &bf, ULW_ALPHA);
}

void widget_redraw(void) { dirty = 1; }
void widget_close(void) { DestroyWindow(hwnd); }

int widget_rect(int *x, int *y, int *w, int *h) {
    RECT r;
    if (!hwnd || !GetWindowRect(hwnd, &r)) return 0;
    if (x) *x = r.left;
    if (y) *y = r.top;
    if (w) *w = r.right - r.left;
    if (h) *h = r.bottom - r.top;
    return 1;
}

int widget_post(const wchar_t *name, int code, int value) {
    HWND h = FindWindowW(CLASS_NAME, name);
    if (!h) return 0;
    PostMessageW(h, MSG_USER, (WPARAM)code, (LPARAM)value);
    return 1;
}

int widget_mouse(int *x, int *y) {
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(hwnd, &p);
    if (x) *x = p.x / S;
    if (y) *y = p.y / S;
    return p.x >= 0 && p.y >= 0 && p.x < PW && p.y < PH;
}

static void update_timer(void) {
    int want = (fps > 0 && !hidden) ? 1000 / fps : 0;
    if (want == timer_period) return;
    timer_period = want;
    if (!want) { CancelWaitableTimer(timer); return; }
    LARGE_INTEGER due;
    due.QuadPart = -10000LL * want;
    SetWaitableTimer(timer, &due, want, NULL, NULL, FALSE);
    last_tick = now_seconds();
}

void widget_set_fps(int f) {
    fps = f;
    if (hwnd) update_timer();
}

static void set_hidden(int h) {
    if (h == hidden) return;
    hidden = h;
    ShowWindow(hwnd, h ? SW_HIDE : SW_SHOWNOACTIVATE);
    update_timer();
    if (D->on_hide) D->on_hide(h);
    if (!h) dirty = 1;
}

static void try_hotkeys(void) {
    if (own_hotkeys) return;
    if (RegisterHotKey(hwnd, HK_HIDEALL, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'B')) {
        own_hotkeys = 1;
        RegisterHotKey(hwnd, HK_QUITALL, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'Q');
    }
}

static void save_state(void) {
    RECT r;
    GetWindowRect(hwnd, &r);
    cfg_set_int(L"x", r.left);
    cfg_set_int(L"y", r.top);
    cfg_set_int(L"topmost", topmost);
    cfg_set_int(L"opacity", opacity);
    cfg_set_int(L"dim", dim);
}

static void show_menu(void) {
    HMENU m = CreatePopupMenu(), op = CreatePopupMenu();
    static const int levels[] = { 100, 75, 50, 35, 25, 15 };
    for (int i = 0; i < 6; i++) {
        wchar_t b[8];
        wsprintfW(b, L"%d%%", levels[i]);
        AppendMenuW(op, MF_STRING | (opacity == levels[i] ? MF_CHECKED : 0), ID_OPA + levels[i], b);
    }
    if (D->menu) {
        HMENU stack[4] = { m };
        int depth = 0;
        for (int i = 0; D->menu[i]; i++) {
            const wchar_t *t = D->menu[i];
            if (t[0] == L'>' && depth < 3) {
                HMENU sub = CreatePopupMenu();
                AppendMenuW(stack[depth], MF_POPUP, (UINT_PTR)sub, t + 1);
                stack[++depth] = sub;
            } else if (t[0] == L'<' && !t[1]) {
                if (depth) depth--;
            } else if (t[0] == L'-' && !t[1]) {
                AppendMenuW(stack[depth], MF_SEPARATOR, 0, NULL);
            } else {
                AppendMenuW(stack[depth], MF_STRING | (D->menu_checked && D->menu_checked(i) ? MF_CHECKED : 0),
                            ID_CUSTOM + i, t);
            }
        }
        AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    }
    AppendMenuW(m, MF_STRING | (topmost ? MF_CHECKED : 0), ID_TOP, L"Siempre encima");
    AppendMenuW(m, MF_STRING | (dim ? MF_CHECKED : 0), ID_DIM, L"Atenuar sin el mouse");
    wchar_t opl[40];
    wsprintfW(opl, L"Opacidad (%d%%, rueda del mouse)", opacity);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)op, opl);
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, ID_HIDEALL, L"Ocultar todos\tCtrl+Alt+B");
    AppendMenuW(m, MF_STRING, ID_CLOSE, L"Cerrar\tClic central");
    AppendMenuW(m, MF_STRING, ID_QUITALL, L"Cerrar todos\tCtrl+Alt+Q");
    POINT p;
    GetCursorPos(&p);
    SetForegroundWindow(hwnd);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, p.x, p.y, 0, hwnd, NULL);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(m);

    if (cmd >= ID_CUSTOM) {
        if (D->on_menu) D->on_menu(cmd - ID_CUSTOM);
        dirty = 1;
    } else if (cmd == ID_TOP) {
        topmost = !topmost;
        SetWindowPos(hwnd, topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else if (cmd == ID_DIM) {
        dim = !dim;
        dirty = 1;
    } else if (cmd >= ID_OPA && cmd <= ID_OPA + 100) {
        opacity = cmd - ID_OPA;
        dirty = 1;
    } else if (cmd == ID_HIDEALL) {
        PostMessageW(HWND_BROADCAST, MSG_HIDEALL, 1, 0);
    } else if (cmd == ID_CLOSE) {
        close_by_user = 1;
        DestroyWindow(hwnd);
    } else if (cmd == ID_QUITALL) {
        PostMessageW(HWND_BROADCAST, MSG_QUITALL, 0, 0);
    }
    if (cmd) save_state();
}

/* ---------------------------------------------------------------- iman
 * Al arrastrar, el widget se pega (a menos de SNAP px) a los bordes del area de
 * trabajo y a los otros widgets visibles: queda a GAP px de ellos y alinea sus
 * bordes. Como los tamanos son multiplos de GRID, los bordes coinciden. */
#define SNAP 12
#define GAP 0           /* sin separacion: cada widget ya deja MARGIN por dentro */

typedef struct { RECT *r; RECT others[32]; int n; } SnapCtx;

static BOOL CALLBACK collect_widget(HWND w, LPARAM lp) {
    SnapCtx *c = (SnapCtx *)lp;
    wchar_t cls[32];
    if (w == hwnd || !IsWindowVisible(w) || c->n >= 32) return TRUE;
    if (!GetClassNameW(w, cls, 32) || lstrcmpW(cls, CLASS_NAME)) return TRUE;
    GetWindowRect(w, &c->others[c->n++]);
    return TRUE;
}

/* prueba un destino para un borde: guarda el desplazamiento mas chico dentro de SNAP */
static void try_snap(int edge, int target, int *best) {
    int d = target - edge;
    if ((d < 0 ? -d : d) < (*best < 0 ? -*best : *best)) *best = d;
}

static void snap_rect(RECT *r) {
    if (GetKeyState(VK_SHIFT) < 0) return;      /* Shift al arrastrar: sin iman */
    int snap = SNAP * S, gap = GAP * S, inf = snap + 1;
    int bx = inf, by = inf;
    MONITORINFO mi = { sizeof mi };
    GetMonitorInfoW(MonitorFromRect(r, MONITOR_DEFAULTTONEAREST), &mi);
    RECT wa = mi.rcWork;
    try_snap(r->left, wa.left + gap, &bx);
    try_snap(r->right, wa.right - gap, &bx);
    try_snap(r->top, wa.top + gap, &by);
    try_snap(r->bottom, wa.bottom - gap, &by);

    SnapCtx c = { r };
    EnumWindows(collect_widget, (LPARAM)&c);
    for (int i = 0; i < c.n; i++) {
        RECT *o = &c.others[i];
        /* solo widgets cercanos: que se toquen o casi (margen gap + snap) */
        int m = gap + snap;
        if (r->right < o->left - m || r->left > o->right + m || r->bottom < o->top - m || r->top > o->bottom + m) continue;
        int side_by_side = r->bottom > o->top - snap && r->top < o->bottom + snap;   /* uno al lado del otro */
        int stacked = r->right > o->left - snap && r->left < o->right + snap;         /* uno encima del otro */
        if (side_by_side) {
            try_snap(r->left, o->right + gap, &bx);     /* a su derecha */
            try_snap(r->right, o->left - gap, &bx);     /* a su izquierda */
            try_snap(r->top, o->top, &by);              /* alinear arriba */
            try_snap(r->bottom, o->bottom, &by);        /* alinear abajo */
        }
        if (stacked) {
            try_snap(r->top, o->bottom + gap, &by);     /* debajo */
            try_snap(r->bottom, o->top - gap, &by);     /* encima */
            try_snap(r->left, o->left, &bx);            /* alinear izquierda */
            try_snap(r->right, o->right, &bx);          /* alinear derecha */
        }
    }
    if (bx != inf) { r->left += bx; r->right += bx; }
    if (by != inf) { r->top += by; r->bottom += by; }
}

static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    /* Mensajes compartidos entre todos los widgets (via HWND_BROADCAST). */
    if (msg == MSG_HIDEALL && msg) { set_hidden((int)wp); return 0; }
    if (msg == MSG_QUITALL && msg) { DestroyWindow(h); return 0; }
    if (msg == MSG_REHOOK && msg) { try_hotkeys(); return 0; }
    if (msg == MSG_USER && msg) {
        if (D->on_message) D->on_message((int)wp, (int)lp);
        return 0;
    }

    switch (msg) {
    case WM_HOTKEY:
        if (wp == HK_HIDEALL) PostMessageW(HWND_BROADCAST, MSG_HIDEALL, !hidden, 0);
        else if (wp == HK_QUITALL) PostMessageW(HWND_BROADCAST, MSG_QUITALL, 0, 0);
        return 0;
    case WM_SHOWME:
        set_hidden(0);
        SetForegroundWindow(h);
        return 0;
    case WM_LBUTTONDOWN: {
        int x = (short)LOWORD(lp) / S, y = (short)HIWORD(lp) / S;
        if ((wp & MK_CONTROL) || !D->on_mouse || !D->on_mouse(x, y, MOUSE_DOWN)) {
            ReleaseCapture();
            SendMessageW(h, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }
        return 0;
    }
    case WM_LBUTTONUP:
        if (D->on_mouse) D->on_mouse((short)LOWORD(lp) / S, (short)HIWORD(lp) / S, MOUSE_UP);
        return 0;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        if (D->on_focus) { D->on_focus(msg == WM_SETFOCUS); dirty = 1; }
        return 0;
    case WM_MOUSEWHEEL: {               /* rueda sobre el widget: opacidad de 5 en 5 */
        int o = opacity + ((short)HIWORD(wp) > 0 ? 5 : -5);
        opacity = o < 10 ? 10 : (o > 100 ? 100 : o);
        dirty = 1;
        cfg_set_int(L"opacity", opacity);
        return 0;
    }
    case WM_MBUTTONUP:
        close_by_user = 1;
        DestroyWindow(h);
        return 0;
    case WM_RBUTTONUP:
        show_menu();
        return 0;
    case WM_KEYDOWN: {
        /* Esc NO cierra el widget (preferencia del usuario): es facil pulsarlo sin querer */
        if (D->on_key) D->on_key((int)wp, (int)((lp >> 16) & 0xFF), (lp & (1 << 30)) ? 2 : 1);
        return 0;
    }
    case WM_CHAR:
        if (D->on_char) D->on_char((int)wp);
        return 0;
    case WM_KEYUP:
        if (D->on_key) D->on_key((int)wp, (int)((lp >> 16) & 0xFF), 0);
        return 0;
    case WM_MOUSEMOVE:
        if (!hover) {
            TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 };
            TrackMouseEvent(&t);
            hover = 1;
            if (dim) dirty = 1;
        }
        return 0;
    case WM_MOUSELEAVE:
        hover = 0;
        if (dim) dirty = 1;
        return 0;
    case WM_MOVING:
        snap_rect((RECT *)lp);
        return TRUE;
    case WM_EXITSIZEMOVE:
        save_state();
        return 0;
    case WM_DESTROY:
        save_state();
        if (D->on_close) D->on_close(close_by_user);
        if (own_hotkeys) {
            UnregisterHotKey(h, HK_HIDEALL);
            UnregisterHotKey(h, HK_QUITALL);
            PostMessageW(HWND_BROADCAST, MSG_REHOOK, 0, 0);
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static HDC make_dib(int w, int h, uint32_t **bits) {
    BITMAPINFO bi = { 0 };
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; /* top-down */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)bits, NULL, 0);
    SelectObject(dc, bmp);
    return dc;
}

int widget_run(const WidgetDesc *d) {
    D = d;
    QueryPerformanceFrequency(&qpf);

    /* Modo foto (para pruebas): FIDGETS_SNAPSHOT=archivo dibuja una vez SIN crear
     * ventana y guarda el buffer. FIDGETS_KEYS="17,24,..." (scan codes en hex) se
     * envian antes a on_key. Formato: "BSNP", ancho, alto (int32) y pixeles BGRA
     * premultiplicados, de arriba a abajo. */
    wchar_t snap[MAX_PATH];
    int snapshot = GetEnvironmentVariableW(L"FIDGETS_SNAPSHOT", snap, MAX_PATH) > 0;

    /* Una sola instancia por widget: si ya existe, se muestra y salimos. */
    HWND prev = snapshot ? NULL : FindWindowW(CLASS_NAME, d->name);
    if (prev) { PostMessageW(prev, WM_SHOWME, 0, 0); return 0; }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
    S = ((int)GetDpiForSystem() + 48) / 96;
    if (S < 1) S = 1;
    PW = d->w * S;
    PH = d->h * S;

    find_ini();

    LARGE_INTEGER seed;
    QueryPerformanceCounter(&seed);
    rng_state ^= (uint32_t)seed.QuadPart ^ (GetCurrentProcessId() << 16);
    if (!rng_state) rng_state = 1;

    MSG_HIDEALL = RegisterWindowMessageW(L"fidgets.hideall");
    MSG_QUITALL = RegisterWindowMessageW(L"fidgets.quitall");
    MSG_REHOOK = RegisterWindowMessageW(L"fidgets.rehook");
    MSG_USER = RegisterWindowMessageW(L"fidgets.user");

    topmost = cfg_get_int(L"topmost", 1);
    opacity = cfg_get_int(L"opacity", 100);
    dim = cfg_get_int(L"dim", 0);
    if (opacity < 10 || opacity > 100) opacity = 100;

    /* Posicion: la guardada, si sigue cayendo en algun monitor; si no, abajo a la derecha. */
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int x = cfg_get_int(L"x", wa.right - PW - 24), y = cfg_get_int(L"y", wa.bottom - PH - 24);
    RECT wr = { x, y, x + PW, y + PH };
    if (!MonitorFromRect(&wr, MONITOR_DEFAULTTONULL)) { x = wa.right - PW - 24; y = wa.bottom - PH - 24; }

    memdc = make_dib(PW, PH, &px);
    txtdc = make_dib(PW, PH, &txtpx);
    SetTextColor(txtdc, RGB(255, 255, 255));
    SetBkMode(txtdc, TRANSPARENT);

    if (snapshot) {
        wchar_t keys[512];
        fps = d->fps;
        if (d->on_init) d->on_init();
        if (d->on_key && GetEnvironmentVariableW(L"FIDGETS_KEYS", keys, 512) > 0) {
            int v = 0, have = 0;
            for (wchar_t *p = keys;; p++) {
                int h = *p >= L'0' && *p <= L'9' ? *p - L'0' : (*p >= L'a' && *p <= L'f' ? *p - L'a' + 10 :
                        (*p >= L'A' && *p <= L'F' ? *p - L'A' + 10 : -1));
                if (h >= 0) { v = v * 16 + h; have = 1; continue; }
                if (have) { d->on_key(0, v, 1); v = 0; have = 0; }
                if (!*p) break;
            }
        }
        wchar_t text[1024];
        if (d->on_char && GetEnvironmentVariableW(L"FIDGETS_TEXT", text, 1024) > 0) {
            if (d->on_focus) d->on_focus(1);
            for (wchar_t *p = text; *p; p++) {
                int ch = *p;
                if (ch == L'\\' && p[1]) {
                    p++;
                    ch = *p == L'b' ? 8 : *p == L't' ? 9 : *p == L'n' ? 13 : *p;
                }
                d->on_char(ch);
            }
        }
        /* FIDGETS_TICK_MS: tiempo simulado tras las teclas (por defecto 1000 ms), en pasos de 16 ms */
        int tick_ms = 1000;
        wchar_t tk[16];
        if (GetEnvironmentVariableW(L"FIDGETS_TICK_MS", tk, 16) > 0) {
            tick_ms = 0;
            for (wchar_t *p = tk; *p >= L'0' && *p <= L'9'; p++) tick_ms = tick_ms * 10 + (*p - L'0');
        }
        for (int t = 0; t < tick_ms && d->on_tick && fps > 0; t += 16)
            d->on_tick((float)(tick_ms - t < 16 ? tick_ms - t : 16) / 1000.0f);
        if (d->on_draw) d->on_draw();
        HANDLE f = CreateFileW(snap, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (f == INVALID_HANDLE_VALUE) return 1;
        int32_t hdr[3] = { 0x504E5342, PW, PH }; /* "BSNP" */
        DWORD wr2;
        WriteFile(f, hdr, sizeof hdr, &wr2, NULL);
        WriteFile(f, px, (DWORD)(PW * PH * 4), &wr2, NULL);
        CloseHandle(f);
        return 0;
    }

    HINSTANCE inst = GetModuleHandleW(NULL);
    WNDCLASSEXW wc = { sizeof wc };
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wc);

    hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | (topmost ? WS_EX_TOPMOST : 0),
                           CLASS_NAME, d->name, WS_POPUP, x, y, PW, PH, NULL, NULL, inst, NULL);
    if (!hwnd) return 1;

    timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer) timer = CreateWaitableTimerW(NULL, FALSE, NULL);

    fps = d->fps;
    if (d->on_init) d->on_init();
    render();
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    try_hotkeys();
    update_timer();

    MSG m;
    for (;;) {
        if (dirty && !hidden) render();
        DWORD n = timer_period ? 1 : 0;
        DWORD r = MsgWaitForMultipleObjectsEx(n, &timer, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (n && r == WAIT_OBJECT_0) {
            double t = now_seconds();
            float dt = (float)(t - last_tick);
            last_tick = t;
            if (dt > 0.1f) dt = 0.1f;
            if (D->on_tick) D->on_tick(dt);
            dirty = 1;
        }
        while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) return (int)m.wParam;
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
}
