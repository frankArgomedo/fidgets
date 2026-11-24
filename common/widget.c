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
#include <wtsapi32.h>
#include <math.h>
#include "widget.h"

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

#define CLASS_NAME L"fidgets_widget"
#define HK_HIDEALL 1
#define HK_QUITALL 2
#define WM_SHOWME (WM_APP + 1)

enum { ID_TOP = 100, ID_DIM, ID_HIDEALL, ID_CLOSE, ID_QUITALL, ID_LANG_AUTO, ID_LANG_ES, ID_LANG_EN,
       ID_HELP, ID_OPA = 200, ID_CUSTOM = 1000 };

static const WidgetDesc *D;
static HWND hwnd;
static HDC memdc, txtdc;
static uint32_t *px, *txtpx;
static int PW, PH, S = 1;           /* tamaño físico y escala */
static int fps, dirty, hidden, hover, own_hotkeys;
static int suspended, locked;       /* PC suspendida / sesión bloqueada */
static int unusable;                /* oculto, suspendido o bloqueado: lo último que se avisó a on_hide */
static int topmost = 1, opacity = 100, dim = 0;
static int ui_lang;                 /* ajuste: 0 = automático, o el LANGID primario (10 es, 9 en) */
static int lang = LANG_ES;          /* idioma en uso */
static int close_by_user;           /* 1 si el usuario cerró este widget (clic central, menú) */
static HANDLE timer;
static int timer_period;            /* ms; 0 = apagado */
static double last_tick;
static wchar_t ini_path[MAX_PATH];
static UINT MSG_HIDEALL, MSG_QUITALL, MSG_REHOOK, MSG_USER, MSG_LANG;
static LARGE_INTEGER qpf;
static uint32_t rng_state = 2463534242u;
static int snapshot;                /* modo foto: sin ventana, reloj simulado */
static double snap_clock = 1000.0;

/* ------------------------------------------------------------------ util */

double now_seconds(void) {
    if (snapshot) return snap_clock;
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

/* borra todas las claves de la sección del widget que empiezan con prefix */
void cfg_delete_prefix(const wchar_t *prefix) {
    static wchar_t buf[8192];
    DWORD n = GetPrivateProfileSectionW(D->name, buf, 8192, ini_path);
    int len = lstrlenW(prefix);
    for (wchar_t *p = buf; n && *p; p += lstrlenW(p) + 1) {
        if (CompareStringOrdinal(p, len, prefix, len, FALSE) != CSTR_EQUAL) continue;
        wchar_t *eq = p;
        while (*eq && *eq != L'=') eq++;
        wchar_t c = *eq;
        *eq = 0;
        WritePrivateProfileStringW(D->name, p, NULL, ini_path);
        *eq = c;
    }
}

int cfg_get_range(const wchar_t *key, int def, int min, int max) {
    int v = cfg_get_int(key, def);
    return v < min || v > max ? def : v;
}

int cfg_get_choice(const wchar_t *key, int def, const int *vals, int n) {
    int v = cfg_get_int(key, def);
    for (int i = 0; i < n; i++) if (vals[i] == v) return v;
    return def;
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
    .text        = RGBA(240, 242, 248, 240),
    .sub         = RGBA(150, 156, 170, 215),
    .accent      = RGBA(250, 190, 70, 240),
    .error       = RGBA(235, 80, 80, 240),
    .extra       = RGBA(160, 55, 60, 230),
    .surface     = RGBA(52, 55, 64, 230),
    .surface_alt = RGBA(55, 75, 135, 225),
    .line        = RGBA(255, 255, 255, 40),
    .ok          = RGBA(50, 195, 170, 235),   /* verde azulado: se distingue de error con daltonismo rojo-verde */
    .ink         = RGBA(16, 18, 24, 230),
};
const Theme *theme = &default_theme;

Color color_mix(Color a, Color b, float t) {
    if (t <= 0) return a;
    if (t >= 1) return b;
    int r = (int)(((a >> 16) & 255) * (1 - t) + ((b >> 16) & 255) * t);
    int g = (int)(((a >> 8) & 255) * (1 - t) + ((b >> 8) & 255) * t);
    int bl = (int)((a & 255) * (1 - t) + (b & 255) * t);
    int al = (int)((a >> 24) * (1 - t) + (b >> 24) * t);
    return RGBA(r, g, bl, al);
}

Color color_alpha(Color c, int a) {
    if (a < 0) a = 0;
    if (a > 255) a = 255;
    return (c & 0xFFFFFF) | ((Color)a << 24);
}

/* --------------------------------------------------------------- idiomas */

enum { B_ON_TOP, B_DIM, B_OPACITY, B_LANGUAGE, B_LANG_AUTO, B_HELP, B_HIDE_ALL, B_CLOSE, B_CLOSE_ALL, B_COUNT };
static const wchar_t *const BASE_TXT[LANG_COUNT][TB_COUNT + B_COUNT] = {
    [LANG_ES] = {
        [TB_PAUSED] = L"en pausa · clic para seguir",
        [TB_NO_KEYBOARD] = L"sin teclado · clic para seguir",
        [TB_COUNT + B_ON_TOP] = L"Siempre encima",
        [TB_COUNT + B_DIM] = L"Atenuar sin el mouse",
        [TB_COUNT + B_OPACITY] = L"Opacidad (%d%%)",
        [TB_COUNT + B_LANGUAGE] = L"Idioma",
        [TB_COUNT + B_LANG_AUTO] = L"Automático",
        [TB_COUNT + B_HELP] = L"Controles",
        [TB_COUNT + B_HIDE_ALL] = L"Ocultar todos\tCtrl+Alt+B",
        [TB_COUNT + B_CLOSE] = L"Cerrar\tClic central",
        [TB_COUNT + B_CLOSE_ALL] = L"Cerrar todos\tCtrl+Alt+Q",
    },
    [LANG_EN] = {
        [TB_PAUSED] = L"paused · click to resume",
        [TB_NO_KEYBOARD] = L"no keyboard · click to resume",
        [TB_COUNT + B_ON_TOP] = L"Always on top",
        [TB_COUNT + B_DIM] = L"Dim when the mouse is away",
        [TB_COUNT + B_OPACITY] = L"Opacity (%d%%)",
        [TB_COUNT + B_LANGUAGE] = L"Language",
        [TB_COUNT + B_LANG_AUTO] = L"Automatic",
        [TB_COUNT + B_HELP] = L"Controls",
        [TB_COUNT + B_HIDE_ALL] = L"Hide all\tCtrl+Alt+B",
        [TB_COUNT + B_CLOSE] = L"Close\tMiddle click",
        [TB_COUNT + B_CLOSE_ALL] = L"Close all\tCtrl+Alt+Q",
    },
};

static const wchar_t *base_txt(int id) {
    const wchar_t *s = BASE_TXT[lang][id];
    return s ? s : BASE_TXT[LANG_ES][id];
}
const wchar_t *tr_base(int id) { return id >= 0 && id < TB_COUNT ? base_txt(id) : L""; }
static const wchar_t *menu_txt(int b) { return base_txt(TB_COUNT + b); }

const wchar_t *tr(int id) {
    if (!D || id <= 0 || id >= D->ntexts) return L"";
    const wchar_t *s = D->texts[lang] ? D->texts[lang][id] : 0;
    if (!s) s = D->texts[LANG_ES] ? D->texts[LANG_ES][id] : 0;
    return s ? s : L"";
}

int widget_lang(void) { return lang; }

/* ui_lang: 0 = el de Windows (español si Windows está en español; si no, inglés) */
static void apply_lang(void) {
    int p = ui_lang ? ui_lang : (int)(GetUserDefaultUILanguage() & 0x3ff);
    lang = p == LANG_SPANISH ? LANG_ES : LANG_EN;
}

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

/* Texto que se parte en dos líneas si no cabe a lo ancho del widget (dentro del
 * margen): se corta en " · " o, si no hay, en el último espacio antes de la mitad. */
static int split_at(const wchar_t *s, int *skip) {
    int n = lstrlenW(s), best = -1;
    for (int i = 0; i + 2 < n; i++)
        if (s[i] == L' ' && s[i + 1] == 0x00B7 && s[i + 2] == L' ') { *skip = 3; return i; }
    for (int i = 0; i < n; i++) if (s[i] == L' ' && (best < 0 || i <= n / 2)) best = i;
    *skip = 1;
    return best;
}

int gfx_text_lines(const wchar_t *s, int size, int flags) {
    int skip;
    if (gfx_text_width(s, -1, size, flags) + 2 <= PW / S - 2 * MARGIN) return 1;   /* +2: sombra y redondeo */
    return split_at(s, &skip) >= 0 ? 2 : 1;
}

int gfx_text_wrap(int x, int y, const wchar_t *s, Color c, int size, int align) {
    int skip, at;
    if (gfx_text_lines(s, size, align) == 1 || (at = split_at(s, &skip)) < 0) {
        gfx_text(x, y, s, c, size, align);
        return 1;
    }
    wchar_t a[128];
    lstrcpynW(a, s, at + 1 < 128 ? at + 1 : 128);
    gfx_text(x, y, a, c, size, align);
    gfx_text(x, y + size * 3 / 2, s + at + skip, c, size, align);
    return 2;
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

int widget_reduce_motion(void) {
    BOOL anim = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &anim, 0);
    return !anim;
}

static void update_timer(void) {
    int want = (fps > 0 && !unusable) ? 1000 / fps : 0;
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

/* Avisa a on_hide cuando el widget deja de poder usarse (oculto, PC suspendida o
 * sesión bloqueada) o vuelve a poder usarse. Para el widget, los tres son lo mismo. */
static void update_usable(void) {
    int u = hidden || suspended || locked;
    if (u == unusable) return;
    unusable = u;
    if (hwnd) update_timer();
    if (D->on_hide) D->on_hide(u);
    if (!u) { dirty = 1; last_tick = now_seconds(); }
}

static void set_hidden(int h) {
    if (h == hidden) return;
    hidden = h;
    ShowWindow(hwnd, h ? SW_HIDE : SW_SHOWNOACTIVATE);
    update_usable();
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
    cfg_set_int(L"ui_lang", ui_lang);
}

/* agrega una opción con su marca: CHECK_ON = ✓, CHECK_RADIO = • */
static void add_item(HMENU m, UINT id, const wchar_t *text, int check) {
    AppendMenuW(m, MF_STRING | (check ? MF_CHECKED : 0), id, text);
    if (check == CHECK_RADIO) {
        MENUITEMINFOW mi = { sizeof mi, MIIM_FTYPE };
        mi.fType = MFT_STRING | MFT_RADIOCHECK;
        SetMenuItemInfoW(m, id, FALSE, &mi);
    }
}

static void show_menu(void) {
    HMENU m = CreatePopupMenu(), op = CreatePopupMenu(), lm = CreatePopupMenu();
    static const int levels[] = { 100, 75, 50, 35, 25, 15 };
    for (int i = 0; i < 6; i++) {
        wchar_t b[8];
        wsprintfW(b, L"%d%%", levels[i]);
        add_item(op, ID_OPA + levels[i], b, opacity == levels[i] ? CHECK_RADIO : CHECK_NONE);
    }
    if (D->menu) {
        HMENU stack[4] = { m };
        int depth = 0;
        for (int i = 0; D->menu[i]; i++) {
            int e = D->menu[i];
            if (e == MENU_END) {
                if (depth) depth--;
            } else if (e == MENU_SEP) {
                AppendMenuW(stack[depth], MF_SEPARATOR, 0, NULL);
            } else if ((e & MENU_SUB) && depth < 3) {
                HMENU sub = CreatePopupMenu();
                AppendMenuW(stack[depth], MF_POPUP, (UINT_PTR)sub, tr(e & ~MENU_SUB));
                stack[++depth] = sub;
            } else {
                add_item(stack[depth], ID_CUSTOM + i, tr(e), D->menu_checked ? D->menu_checked(i) : CHECK_NONE);
            }
        }
        AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    }
    if (D->help && D->help[0]) {     /* «Controles ▸»: líneas grises, solo para leer */
        HMENU hm = CreatePopupMenu();
        for (int i = 0; D->help[i]; i++) AppendMenuW(hm, MF_STRING | MF_GRAYED, ID_HELP, tr(D->help[i]));
        AppendMenuW(m, MF_POPUP, (UINT_PTR)hm, menu_txt(B_HELP));
    }
    add_item(m, ID_TOP, menu_txt(B_ON_TOP), topmost);
    add_item(m, ID_DIM, menu_txt(B_DIM), dim);
    wchar_t opl[48];
    wsprintfW(opl, menu_txt(B_OPACITY), opacity);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)op, opl);
    add_item(lm, ID_LANG_AUTO, menu_txt(B_LANG_AUTO), ui_lang == 0 ? CHECK_RADIO : CHECK_NONE);
    add_item(lm, ID_LANG_ES, L"Español", ui_lang == LANG_SPANISH ? CHECK_RADIO : CHECK_NONE);
    add_item(lm, ID_LANG_EN, L"English", ui_lang == LANG_ENGLISH ? CHECK_RADIO : CHECK_NONE);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)lm, menu_txt(B_LANGUAGE));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, ID_HIDEALL, menu_txt(B_HIDE_ALL));
    AppendMenuW(m, MF_STRING, ID_CLOSE, menu_txt(B_CLOSE));
    AppendMenuW(m, MF_STRING, ID_QUITALL, menu_txt(B_CLOSE_ALL));
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
    } else if (cmd >= ID_LANG_AUTO && cmd <= ID_LANG_EN) {   /* el idioma cambia en todos los widgets */
        int v = cmd == ID_LANG_ES ? LANG_SPANISH : cmd == ID_LANG_EN ? LANG_ENGLISH : 0;
        PostMessageW(HWND_BROADCAST, MSG_LANG, (WPARAM)v, 0);
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
    if (msg == MSG_LANG && msg) {
        ui_lang = (int)wp;
        cfg_set_int(L"ui_lang", ui_lang);
        apply_lang();
        dirty = 1;
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
    /* La rueda queda libre para el widget; la opacidad solo va en el menú. */
    case WM_POWERBROADCAST:
        if (wp == PBT_APMSUSPEND) { suspended = 1; update_usable(); }
        else if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) { suspended = 0; update_usable(); }
        return TRUE;
    case WM_WTSSESSION_CHANGE:
        if (wp == WTS_SESSION_LOCK) { locked = 1; update_usable(); }
        else if (wp == WTS_SESSION_UNLOCK) { locked = 0; update_usable(); }
        return 0;
    case WM_MBUTTONUP:
        close_by_user = 1;
        DestroyWindow(h);
        return 0;
    case WM_RBUTTONUP:
        show_menu();
        return 0;
    case WM_KEYDOWN: {
        /* Esc NO cierra el widget (preferencia del usuario): es fácil pulsarlo sin querer */
        int scan = (int)((lp >> 16) & 0xFF) | ((lp & (1 << 24)) ? SCAN_EXTENDED : 0);
        if (D->on_key) D->on_key((int)wp, scan, (lp & (1 << 30)) ? 2 : 1);
        return 0;
    }
    case WM_CHAR:
        if (D->on_char) D->on_char((int)wp);
        return 0;
    case WM_KEYUP: {
        int scan = (int)((lp >> 16) & 0xFF) | ((lp & (1 << 24)) ? SCAN_EXTENDED : 0);
        if (D->on_key) D->on_key((int)wp, scan, 0);
        return 0;
    }
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
        WTSUnRegisterSessionNotification(h);
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

/* ------------------------------------------------------------- modo foto
 * Dibuja sin ventana tras un guion de eventos (FIDGETS_EVENTS, ver widget.h) y
 * guarda el buffer en FIDGETS_SNAPSHOT. Formato: "BSNP", ancho, alto (int32) y
 * píxeles BGRA premultiplicados, de arriba a abajo. */
static int snap_focused;

static void snap_tick(int ms) {
    for (int t = 0; t < ms; t += 16) {
        int step = ms - t < 16 ? ms - t : 16;
        snap_clock += step / 1000.0;
        if (D->on_tick && fps > 0 && !unusable) D->on_tick((float)step / 1000.0f);
    }
}

static void snap_focus(int f) {
    if (f == snap_focused) return;
    snap_focused = f;
    if (D->on_focus) D->on_focus(f);
}

static int read_int(const wchar_t **p, int base) {
    int v = 0;
    for (;; (*p)++) {
        int c = **p, h = c >= L'0' && c <= L'9' ? c - L'0' :
                (base == 16 && c >= L'a' && c <= L'f') ? c - L'a' + 10 :
                (base == 16 && c >= L'A' && c <= L'F') ? c - L'A' + 10 : -1;
        if (h < 0) return v;
        v = v * base + h;
    }
}

static int starts(const wchar_t *p, const wchar_t *w) {
    while (*w) if (*p++ != *w++) return 0;
    return 1;
}

static void run_events(const wchar_t *p) {
    while (*p) {
        while (*p == L' ') p++;
        if (!*p) break;
        if (starts(p, L"key:")) {
            p += 4;
            for (;;) {
                int v = read_int(&p, 16);
                int vk = (int)MapVirtualKeyW((UINT)((v & SCAN_EXTENDED) ? 0xE000 | (v & 0xFF) : v), MAPVK_VSC_TO_VK_EX);
                if (D->on_key) D->on_key(vk, v, 1);
                if (*p != L',') break;
                p++;
            }
        } else if (starts(p, L"text:")) {
            p += 5;
            snap_focus(1);   /* escribir implica haber clicado el widget */
            for (; *p && *p != L' '; p++) {
                int ch = *p;
                if (ch == L'\\' && p[1]) {
                    p++;
                    ch = *p == L's' ? L' ' : *p == L'b' ? 8 : *p == L't' ? 9 : *p == L'n' ? 13 : *p;
                }
                if (D->on_char) D->on_char(ch);
                snap_tick(100);
            }
        } else if (starts(p, L"click:")) {
            p += 6;
            int x = read_int(&p, 10);
            if (*p == L',') p++;
            int y = read_int(&p, 10);
            snap_focus(1);
            if (D->on_mouse && D->on_mouse(x, y, MOUSE_DOWN)) D->on_mouse(x, y, MOUSE_UP);
        } else if (starts(p, L"tick:")) {
            p += 5;
            snap_tick(read_int(&p, 10));
        } else if (starts(p, L"hide")) {
            p += 4; hidden = 1; update_usable();
        } else if (starts(p, L"show")) {
            p += 4; hidden = 0; update_usable();
        } else if (starts(p, L"focus")) {
            p += 5; snap_focus(1);
        } else if (starts(p, L"blur")) {
            p += 4; snap_focus(0);
        } else if (starts(p, L"lang:")) {
            p += 5;
            ui_lang = starts(p, L"en") ? LANG_ENGLISH : LANG_SPANISH;
            apply_lang();
            while (*p && *p != L' ') p++;
        } else {
            while (*p && *p != L' ') p++;   /* evento desconocido: se ignora */
        }
    }
}

static int run_snapshot(const wchar_t *file) {
    static wchar_t ev[4096];
    fps = D->fps;
    if (D->on_init) D->on_init();
    if (GetEnvironmentVariableW(L"FIDGETS_EVENTS", ev, 4096) > 0) run_events(ev);
    if (D->on_draw) D->on_draw();
    HANDLE f = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return 1;
    int32_t hdr[3] = { 0x504E5342, PW, PH }; /* "BSNP" */
    DWORD wr;
    WriteFile(f, hdr, sizeof hdr, &wr, NULL);
    WriteFile(f, px, (DWORD)(PW * PH * 4), &wr, NULL);
    CloseHandle(f);
    return 0;
}

/* ----------------------------------------------------------------- inicio */

/* Una sola instancia por name: un mutex con nombre (sin la carrera de buscar la
 * ventana). Si ya existe, se le pide a esa que se muestre y se sale. */
static int already_running(void) {
    wchar_t mx[96];
    wsprintfW(mx, L"Local\\fidgets.%s", D->name);
    CreateMutexW(NULL, FALSE, mx);   /* el sistema lo libera al salir del proceso */
    if (GetLastError() != ERROR_ALREADY_EXISTS) return 0;
    for (int i = 0; i < 40; i++) {   /* la otra puede estar creando su ventana todavía */
        HWND prev = FindWindowW(CLASS_NAME, D->name);
        if (prev) { PostMessageW(prev, WM_SHOWME, 0, 0); break; }
        Sleep(50);
    }
    return 1;
}

int widget_run(const WidgetDesc *d) {
    D = d;
    QueryPerformanceFrequency(&qpf);

    wchar_t snap[MAX_PATH];
    snapshot = GetEnvironmentVariableW(L"FIDGETS_SNAPSHOT", snap, MAX_PATH) > 0;
    if (!snapshot && already_running()) return 0;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
    S = ((int)GetDpiForSystem() + 48) / 96;
    if (S < 1) S = 1;
    PW = d->w * S;
    PH = d->h * S;

    find_ini();

    wchar_t sd[16];
    if (GetEnvironmentVariableW(L"FIDGETS_SEED", sd, 16) > 0) {
        const wchar_t *p = sd;
        rng_state = (uint32_t)read_int(&p, 10);
    } else {
        LARGE_INTEGER seed;
        QueryPerformanceCounter(&seed);
        rng_state ^= (uint32_t)seed.QuadPart ^ (GetCurrentProcessId() << 16);
    }
    if (!rng_state) rng_state = 1;

    MSG_HIDEALL = RegisterWindowMessageW(L"fidgets.hideall");
    MSG_QUITALL = RegisterWindowMessageW(L"fidgets.quitall");
    MSG_REHOOK = RegisterWindowMessageW(L"fidgets.rehook");
    MSG_USER = RegisterWindowMessageW(L"fidgets.user");
    MSG_LANG = RegisterWindowMessageW(L"fidgets.lang");

    static const int LANGS[] = { 0, LANG_SPANISH, LANG_ENGLISH };
    topmost = cfg_get_range(L"topmost", 1, 0, 1);
    opacity = cfg_get_range(L"opacity", 100, 10, 100);
    dim = cfg_get_range(L"dim", 0, 0, 1);
    ui_lang = snapshot ? LANG_SPANISH : cfg_get_choice(L"ui_lang", 0, LANGS, 3);   /* fotos: español salvo lang: */
    apply_lang();

    /* Posición: la guardada, si sigue cayendo en algún monitor; si no, abajo a la derecha. */
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int x = cfg_get_int(L"x", wa.right - PW - 24), y = cfg_get_int(L"y", wa.bottom - PH - 24);
    RECT wr = { x, y, x + PW, y + PH };
    if (!MonitorFromRect(&wr, MONITOR_DEFAULTTONULL)) { x = wa.right - PW - 24; y = wa.bottom - PH - 24; }

    memdc = make_dib(PW, PH, &px);
    txtdc = make_dib(PW, PH, &txtpx);
    SetTextColor(txtdc, RGB(255, 255, 255));
    SetBkMode(txtdc, TRANSPARENT);

    if (snapshot) return run_snapshot(snap);

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
    WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION);   /* bloqueo de sesión */

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
