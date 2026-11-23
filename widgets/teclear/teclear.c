/*
 * teclear - test de mecanografia: escribir palabras al azar contra el reloj o por
 * cantidad, y ver WPM y precision. Mecanica tomada de monkeytype; aspecto propio.
 *
 *   Clic en el widget  -> darle el teclado; el test arranca con la primera letra
 *   Espacio            -> siguiente palabra (a mitad de palabra la salta)
 *   Retroceso          -> corregir; vuelve a la palabra anterior solo si tenia un error
 *   Ctrl+Retroceso     -> borrar la palabra
 *   Tab                -> reiniciar con palabras nuevas (Enter tambien, en el resultado)
 *   Esc                -> cancelar el test en curso (vuelve al inicio, mismas palabras)
 *   Clic derecho       -> modo, duracion, idioma, puntuacion, numeros, tildes, dificultad
 *
 * Calculos (como monkeytype):
 *   WPM   = caracteres de palabras bien escritas (con su espacio) / 5, por minuto
 *   bruto = todo lo tecleado (bien, mal y de mas) / 5, por minuto
 *   precision = teclas correctas / teclas pulsadas (un error cuenta aunque se borre)
 */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include "widget.h"

#pragma clang diagnostic ignored "-Wc23-extensions"

#define W 320          /* cuadricula de 40 px: 8x3 modulos */
#define H 120
#define PAD 8
#define FONT 18          /* texto del test, monoespaciado */
#define TOP 27           /* y de la primera linea */
#define LH 31            /* alto de linea */
#define LINES 3
#ifndef ANIM_FPS
#define ANIM_FPS 45      /* mientras el cursor se desliza */
#endif

/* ------------------------------------------------------------ listas de palabras
 * Los .txt (UTF-8, una por linea, orden de frecuencia) van dentro del .exe con
 * #embed. La lista "200" son las primeras 200 lineas de la de 1000. */
static const char ES_TXT[] = {
#embed "palabras/es_1000.txt"
    , 0 };
static const char EN_TXT[] = {
#embed "palabras/en_1000.txt"
    , 0 };

#define LIST_MAX 1000
typedef struct { const char *utf8; wchar_t *pool; const wchar_t *w[LIST_MAX]; int n; } WordList;
static WordList lists[2] = { { ES_TXT }, { EN_TXT } };

static WordList *get_list(int lang) {
    WordList *L = &lists[lang];
    if (L->pool) return L;
    int n = MultiByteToWideChar(CP_UTF8, 0, L->utf8, -1, NULL, 0);
    L->pool = HeapAlloc(GetProcessHeap(), 0, (size_t)n * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, L->utf8, -1, L->pool, n);
    wchar_t *p = L->pool;
    if (*p == 0xFEFF) p++;
    while (*p && L->n < LIST_MAX) {
        wchar_t *s = p;
        while (*p && *p != L'\n' && *p != L'\r') p++;
        int end = *p != 0;
        while (*p == L'\n' || *p == L'\r') *p++ = 0;
        if (*s) L->w[L->n++] = s;
        if (!end) break;
    }
    return L;
}

/* ------------------------------------------------------------ ajustes */

static const int TIMES[4] = { 15, 30, 60, 120 };
static const int COUNTS[4] = { 10, 25, 50, 100 };
enum { MODE_TIME, MODE_WORDS };
enum { DIFF_NORMAL, DIFF_EXPERT, DIFF_MASTER };
static int mode, time_i = 1, count_i = 1, lang, big, punct, nums, lazy, diff;

/* ------------------------------------------------------------ estado del test */

#define MAXW 640              /* palabras por test (120 s a 300 WPM) */
#define MAXL 40               /* largo maximo de una palabra mostrada */
#define MAXX 20               /* letras de mas permitidas, como monkeytype */
#define MAXT (MAXL + MAXX)

enum { IDLE, RUN, DONE, FAILED };
static int state = IDLE, focused;
static wchar_t text[MAXW][MAXL];
static int tlen[MAXW];
static wchar_t typed[MAXW][MAXT];
static int ylen[MAXW];
static int nwords, cur, prev_base = -1, open_mark;
static int keys_ok, keys_bad;
static double t0;
static int fail_reason;       /* 1 = palabra mal (experto), 2 = tecla mal (maestro) */
static int res_wpm, res_raw, res_acc, res_pb, res_new_pb, res_low_acc;   /* x100 */
static int cur_pb;            /* record de la configuracion actual (x100) */
#define MIN_ACC 7500          /* como monkeytype: con menos de 75% de precision no hay record */

static int total_words(void) { return mode == MODE_WORDS ? COUNTS[count_i] : MAXW; }
static int duration(void) { return TIMES[time_i]; }

static int word_ok(int i) {
    if (ylen[i] != tlen[i]) return 0;
    for (int k = 0; k < tlen[i]; k++) if (typed[i][k] != text[i][k]) return 0;
    return 1;
}

static void strip_accents(wchar_t *s) {
    static const wchar_t from[] = L"áéíóúüÁÉÍÓÚÜ", to[] = L"aeiouuAEIOUU";
    for (; *s; s++)
        for (int k = 0; from[k]; k++) if (*s == from[k]) { *s = to[k]; break; }
}

static int ends_with(const wchar_t *s, int n, const wchar_t *set) {
    if (!n) return 0;
    for (; *set; set++) if (s[n - 1] == *set) return 1;
    return 0;
}

static void put(int i, const wchar_t *pre, const wchar_t *post) {
    wchar_t b[MAXL];
    int n = 0;
    for (const wchar_t *p = pre; *p && n < MAXL - 1; p++) b[n++] = *p;
    for (int k = 0; k < tlen[i] && n < MAXL - 1; k++) b[n++] = text[i][k];
    for (const wchar_t *p = post; *p && n < MAXL - 1; p++) b[n++] = *p;
    b[n] = 0;
    lstrcpyW(text[i], b);
    tlen[i] = n;
}

/* Genera la palabra i. Puntuacion como monkeytype: mayuscula al empezar una
 * oracion (en espanol a veces con ¿ o ¡), fin de oracion 10%, comillas 1%,
 * parentesis 1.2%, dos puntos 1.3%, punto y coma 1.5%, coma 20%. */
static void gen_word(int i) {
    WordList *L = get_list(lang);
    int n = big ? L->n : (L->n < 200 ? L->n : 200);
    int last = i == total_words() - 1;
    if (nums && rnd() % 100 < 10) {
        int d = rnd_range(1, 4), v = 0;
        for (int k = 0; k < d; k++) v = v * 10 + (k == 0 && d > 1 ? rnd_range(1, 9) : rnd_range(0, 9));
        wsprintfW(text[i], L"%d", v);
    } else {
        int b;
        do b = rnd_range(0, n - 1); while (b == prev_base && n > 1);
        prev_base = b;
        lstrcpynW(text[i], L->w[b], MAXL - 4);
        if (lazy) strip_accents(text[i]);
    }
    tlen[i] = lstrlenW(text[i]);
    if (!punct) return;

    int cap = i == 0 || ends_with(text[i - 1], tlen[i - 1], L".?!");
    int digits = text[i][0] >= L'0' && text[i][0] <= L'9';
    if (cap && !digits) {
        CharUpperBuffW(text[i], 1);
        if (lang == 0 && !open_mark) {
            int r = rnd() % 100;
            if (r < 10) { put(i, L"¿", L""); open_mark = L'?'; }
            else if (r < 20) { put(i, L"¡", L""); open_mark = L'!'; }
        }
    }
    if (last) {
        wchar_t e[2] = { open_mark ? (wchar_t)open_mark : L'.', 0 };
        put(i, L"", e);
        open_mark = 0;
        return;
    }
    int prev_stop = i > 0 && ends_with(text[i - 1], tlen[i - 1], L".,?!");
    int r = rnd() % 1000;
    if (!cap && !prev_stop && r < 100) {
        wchar_t e[2] = { L'.', 0 };
        if (open_mark) e[0] = (wchar_t)open_mark;
        else if (lang == 1) { int q = rnd() % 10; e[0] = q == 0 ? L'?' : q == 1 ? L'!' : L'.'; }
        put(i, L"", e);
        open_mark = 0;
    } else if ((r = rnd() % 1000) < 10) put(i, L"\"", L"\"");
    else if (r < 22) put(i, L"(", L")");
    else if (r < 35) put(i, L"", L":");
    else if (r < 50) put(i, L"", L";");
    else if (r < 250) put(i, L"", L",");
}

static void ensure_words(int upto) {
    int lim = total_words();
    if (upto > lim) upto = lim;
    while (nwords < upto) { ylen[nwords] = 0; gen_word(nwords); nwords++; }
}

/* ------------------------------------------------------------ cursor suave */

static float caret_x, caret_y, caret_tx, caret_ty;
static int caret_moving;
static int cw = 8;            /* ancho de un caracter monoespaciado */
static int cols = 36;
static int line_of[MAXW], col_of[MAXW];

static void update_fps(void) {
    if (caret_moving) widget_set_fps(ANIM_FPS);
    else if (state == RUN && mode == MODE_TIME) widget_set_fps(4);   /* cuenta regresiva */
    else widget_set_fps(0);
}

/* posicion de cada palabra (las letras de mas ensanchan la palabra) */
static void layout(int upto) {
    int line = 0, col = 0;
    for (int i = 0; i <= upto && i < nwords; i++) {
        int len = ylen[i] > tlen[i] ? ylen[i] : tlen[i];
        if (col > 0 && col + len > cols) { line++; col = 0; }
        line_of[i] = line;
        col_of[i] = col;
        col += len + 1;
    }
}

static int first_line(void) { return line_of[cur] > 0 ? line_of[cur] - 1 : 0; }

static void caret_target(int snap) {
    layout(cur);
    caret_tx = (float)(PAD + (col_of[cur] + ylen[cur]) * cw);
    caret_ty = (float)(TOP + (line_of[cur] - first_line()) * LH);
    if (snap || caret_ty != caret_y) { caret_x = caret_tx; caret_y = caret_ty; caret_moving = 0; }
    else caret_moving = caret_x != caret_tx;
    update_fps();
    widget_redraw();
}

/* ------------------------------------------------------------ test */

static void pb_key(wchar_t *k);

static void restart(void) {
    wchar_t k[48];
    pb_key(k);
    cur_pb = cfg_get_int(k, 0);
    state = IDLE;
    nwords = cur = keys_ok = keys_bad = 0;
    prev_base = -1;
    open_mark = 0;
    ensure_words(mode == MODE_WORDS ? total_words() : 80);
    caret_target(1);
}

/* Esc: el mismo texto otra vez, sin lo escrito */
static void retry(void) {
    state = IDLE;
    cur = keys_ok = keys_bad = 0;
    for (int i = 0; i < nwords; i++) ylen[i] = 0;
    caret_target(1);
}

static void pb_key(wchar_t *k) {
    wsprintfW(k, L"pb_%c%d_%c%d_%d%d%d%d", mode == MODE_TIME ? L't' : L'w',
              mode == MODE_TIME ? duration() : COUNTS[count_i], lang ? L'e' : L's', big ? 1000 : 200,
              punct, nums, lazy, diff);
}

/* caracteres para WPM y bruto (ver cabecera) */
static void count_chars(int finished_word, int *correct, int *raw) {
    int c = 0, r = 0;
    for (int i = 0; i < cur; i++) {
        r += ylen[i] + 1;
        if (word_ok(i)) c += tlen[i] + 1;
    }
    if (cur < nwords && ylen[cur]) {
        r += ylen[cur];
        int ok = ylen[cur] <= tlen[cur];
        for (int k = 0; ok && k < ylen[cur]; k++) ok = typed[cur][k] == text[cur][k];
        if (ok && (finished_word || mode == MODE_TIME)) c += ylen[cur];   /* credito parcial al final del tiempo */
    }
    *correct = c;
    *raw = r;
}

static void finish(double secs, int finished_word) {
    int c, r;
    count_chars(finished_word, &c, &r);
    if (secs < 0.001) secs = 0.001;
    res_wpm = (int)(c / 5.0 * 60.0 / secs * 100 + 0.5);
    res_raw = (int)(r / 5.0 * 60.0 / secs * 100 + 0.5);
    res_acc = keys_ok + keys_bad ? (int)(keys_ok * 10000.0 / (keys_ok + keys_bad) + 0.5) : 0;
    wchar_t k[48];
    pb_key(k);
    res_pb = cur_pb;
    res_low_acc = res_acc < MIN_ACC;
    res_new_pb = !res_low_acc && res_wpm > res_pb;
    if (res_new_pb) { cfg_set_int(k, res_wpm); cur_pb = res_wpm; }
    state = DONE;
    caret_moving = 0;
    update_fps();
    widget_redraw();
}

static void fail(int why) {
    state = FAILED;
    fail_reason = why;
    caret_moving = 0;
    update_fps();
    widget_redraw();
}

static int time_up(double now) { return state == RUN && mode == MODE_TIME && now - t0 >= duration(); }

static int prev_editable(void) {
    if (cur == 0 || word_ok(cur - 1)) return 0;
    layout(cur);
    return line_of[cur - 1] >= first_line();           /* solo si sigue a la vista */
}

static int on_char(int ch) {
    double now = now_seconds();
    if (time_up(now)) { finish(duration(), 0); return 1; }
    if (ch == 9 || (ch == 13 && state >= DONE)) { restart(); return 1; }
    if (ch == 27) { retry(); return 1; }
    if (state >= DONE) return 1;

    if (ch == 8 || ch == 127) {                          /* retroceso / Ctrl+retroceso */
        if (state != RUN) return 1;
        if (ylen[cur] == 0) {
            if (!prev_editable()) return 1;
            cur--;
            if (ch == 8) { caret_target(0); return 1; }
        }
        if (ch == 127) ylen[cur] = 0; else ylen[cur]--;
        caret_target(0);
        return 1;
    }
    if (ch == L' ') {
        if (state != RUN || ylen[cur] == 0) return 1;    /* espacio al inicio: se ignora */
        int ok = ylen[cur] >= tlen[cur];
        if (ok) keys_ok++; else keys_bad++;
        if (diff == DIFF_MASTER && !ok) { fail(2); return 1; }
        if (diff == DIFF_EXPERT && !word_ok(cur)) { fail(1); return 1; }
        cur++;
        if (mode == MODE_WORDS && cur >= total_words()) { cur = total_words() - 1; finish(now - t0, 1); return 1; }
        ensure_words(cur + 60);
        caret_target(0);
        return 1;
    }
    if (ch < 32) return 0;

    if (state == IDLE) { state = RUN; t0 = now; update_fps(); }
    /* letras de mas: hasta MAXX, y nunca mas alla del ancho de una linea (respeta MARGIN) */
    if (ylen[cur] >= tlen[cur] + MAXX || ylen[cur] >= MAXT || ylen[cur] >= cols) return 1;
    int pos = ylen[cur];
    int ok = pos < tlen[cur] && text[cur][pos] == ch;
    if (ok) keys_ok++; else keys_bad++;
    typed[cur][ylen[cur]++] = (wchar_t)ch;
    if (diff == DIFF_MASTER && !ok) { fail(2); return 1; }
    if (mode == MODE_WORDS && cur == total_words() - 1 && word_ok(cur)) {   /* ultima palabra: termina sin espacio */
        finish(now - t0, 1);
        return 1;
    }
    caret_target(0);
    return 1;
}

static void tick(float dt) {
    if (time_up(now_seconds())) { finish(duration(), 0); return; }
    if (caret_moving) {
        float k = dt * 28;
        if (k > 1) k = 1;
        caret_x += (caret_tx - caret_x) * k;
        if (caret_x - caret_tx < 0.4f && caret_tx - caret_x < 0.4f) { caret_x = caret_tx; caret_moving = 0; update_fps(); }
    }
}

/* ------------------------------------------------------------ dibujo */

static void label(wchar_t *b, int with_pb) {
    if (mode == MODE_TIME) wsprintfW(b, L"%d s", duration());
    else wsprintfW(b, L"%d palabras", COUNTS[count_i]);
    lstrcatW(b, lang ? L" · en" : L" · es");
    if (big) lstrcatW(b, L" 1k");
    if (punct) lstrcatW(b, L" · .,");
    if (nums) lstrcatW(b, L" · 123");
    if (diff == DIFF_EXPERT) lstrcatW(b, L" · experto");
    if (diff == DIFF_MASTER) lstrcatW(b, L" · maestro");
    if (with_pb && cur_pb > 0) wsprintfW(b + lstrlenW(b), L" · récord %d", (cur_pb + 50) / 100);
}

static void ch_at(int x, int y, wchar_t c, Color col) {
    wchar_t s[2] = { c, 0 };
    gfx_text(x, y, s, col, FONT, ALIGN_LEFT | TEXT_MONO | TEXT_SHADOW);
}

static void fmt2(wchar_t *b, int x100) { wsprintfW(b, L"%d", (x100 + 50) / 100); }

static void draw_words(void) {
    layout(nwords - 1);
    int first = first_line();
    for (int i = 0; i < nwords; i++) {
        int l = line_of[i] - first;
        if (l < 0) continue;
        if (l >= LINES) break;
        int x = PAD + col_of[i] * cw, y = TOP + l * LH;
        int len = ylen[i] > tlen[i] ? ylen[i] : tlen[i];
        for (int k = 0; k < len; k++) {
            if (k < ylen[i] && k < tlen[i])
                ch_at(x + k * cw, y, text[i][k], typed[i][k] == text[i][k] ? theme->text : theme->error);
            else if (k < ylen[i]) ch_at(x + k * cw, y, typed[i][k], theme->extra);
            else ch_at(x + k * cw, y, text[i][k], theme->sub);
        }
        if (i < cur && !word_ok(i)) gfx_rect(x, y + LH - 3, len * cw, 1, theme->error);
    }
    if (focused) gfx_rect((int)(caret_x + 0.5f) - 1, (int)caret_y + 3, 2, LH - 7, theme->accent);
}

static void draw_result(void) {
    wchar_t b[64];
    if (state == FAILED) {
        gfx_text(PAD, TOP, L"falló", theme->error, 48, ALIGN_LEFT | TEXT_SHADOW);
        gfx_text(PAD, TOP + 64, fail_reason == 1 ? L"dejaste una palabra con error" : L"tecla incorrecta",
                 theme->sub, 15, ALIGN_LEFT | TEXT_SHADOW);
        return;
    }
    fmt2(b, res_wpm);
    gfx_text(PAD, TOP - 2, b, theme->accent, 64, ALIGN_LEFT | TEXT_SHADOW);
    int x = PAD + gfx_text_width(b, -1, 64, 0) + 4;
    gfx_text(x, TOP + 54, L"wpm", theme->sub, 15, ALIGN_LEFT | TEXT_SHADOW);
    int cx = 160;
    wsprintfW(b, L"precisión %d%%", (res_acc + 50) / 100);
    gfx_text(cx, TOP + 10, b, theme->text, 15, ALIGN_LEFT | TEXT_SHADOW);
    wsprintfW(b, L"bruto %d", (res_raw + 50) / 100);
    gfx_text(cx, TOP + 38, b, theme->sub, 15, ALIGN_LEFT | TEXT_SHADOW);
    Color c = theme->sub;
    if (res_low_acc) { lstrcpyW(b, L"no cuenta (mín. 75%)"); c = theme->error; }
    else if (res_new_pb) { lstrcpyW(b, L"¡nuevo récord!"); c = theme->accent; }
    else if (res_pb > 0) wsprintfW(b, L"récord %d", (res_pb + 50) / 100);
    else b[0] = 0;
    if (b[0]) gfx_text(cx, TOP + 66, b, c, 14, ALIGN_LEFT | TEXT_SHADOW);
}

static void draw(void) {
    wchar_t b[96];
    gfx_clear_hit();
    if (state == RUN && mode == MODE_TIME) {
        int left = duration() - (int)(now_seconds() - t0);
        wsprintfW(b, L"%d", left < 0 ? 0 : left);
        gfx_text(PAD, 4, b, theme->accent, 14, ALIGN_LEFT | TEXT_SHADOW);
    } else if (state == RUN) {
        wsprintfW(b, L"%d/%d", cur, total_words());
        gfx_text(PAD, 4, b, theme->accent, 14, ALIGN_LEFT | TEXT_SHADOW);
    }
    const wchar_t *hint = state >= DONE ? L"tab: repetir" : (!focused ? L"clic para escribir" : 0);
    if (state != RUN) {                     /* el modo (y el record), si caben junto al aviso */
        int room = W - 2 * PAD - (hint ? gfx_text_width(hint, -1, 12, 0) + 10 : 0);
        for (int with_pb = 1; with_pb >= 0; with_pb--) {
            label(b, with_pb && state == IDLE);
            if (gfx_text_width(b, -1, 12, 0) <= room) { gfx_text(PAD, 4, b, theme->sub, 12, ALIGN_LEFT | TEXT_SHADOW); break; }
        }
    }
    if (hint) gfx_text(W - PAD, 4, hint, state >= DONE ? theme->sub : theme->accent, 12, ALIGN_RIGHT | TEXT_SHADOW);

    if (state >= DONE) draw_result();
    else draw_words();
}

/* ------------------------------------------------------------ menu y arranque */

static const wchar_t *const MENU[] = {
    L">Modo",                                                                     /* 0 */
    L"Tiempo 15 s", L"Tiempo 30 s", L"Tiempo 60 s", L"Tiempo 120 s", L"-",        /* 1-5 */
    L"10 palabras", L"25 palabras", L"50 palabras", L"100 palabras", L"<",        /* 6-10 */
    L">Idioma",                                                                   /* 11 */
    L"Español", L"Inglés", L"-",                                                  /* 12-14 */
    L"200 palabras más comunes", L"1000 palabras más comunes", L"<",              /* 15-17 */
    L"Puntuación", L"Números", L"Sin tildes",                                     /* 18-20 */
    L">Dificultad",                                                               /* 21 */
    L"Normal", L"Experto: falla si dejas una palabra mal", L"Maestro: falla con el primer error", L"<", /* 22-25 */
    0,
};

static int menu_checked(int i) {
    if (i >= 1 && i <= 4) return mode == MODE_TIME && time_i == i - 1;
    if (i >= 6 && i <= 9) return mode == MODE_WORDS && count_i == i - 6;
    if (i == 12 || i == 13) return lang == i - 12;
    if (i == 15 || i == 16) return big == i - 15;
    if (i == 18) return punct;
    if (i == 19) return nums;
    if (i == 20) return lazy;
    if (i >= 22 && i <= 24) return diff == i - 22;
    return 0;
}

static void save_cfg(void) {
    cfg_set_int(L"modo", mode);
    cfg_set_int(L"tiempo", time_i);
    cfg_set_int(L"palabras", count_i);
    cfg_set_int(L"idioma", lang);
    cfg_set_int(L"lista", big);
    cfg_set_int(L"puntuacion", punct);
    cfg_set_int(L"numeros", nums);
    cfg_set_int(L"sintildes", lazy);
    cfg_set_int(L"dificultad", diff);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void load_cfg(void) {
    mode = clampi(cfg_get_int(L"modo", MODE_TIME), 0, 1);
    time_i = clampi(cfg_get_int(L"tiempo", 1), 0, 3);
    count_i = clampi(cfg_get_int(L"palabras", 1), 0, 3);
    lang = clampi(cfg_get_int(L"idioma", 0), 0, 1);
    big = clampi(cfg_get_int(L"lista", 0), 0, 1);
    punct = cfg_get_int(L"puntuacion", 0) != 0;
    nums = cfg_get_int(L"numeros", 0) != 0;
    lazy = cfg_get_int(L"sintildes", 0) != 0;
    diff = clampi(cfg_get_int(L"dificultad", 0), 0, 2);
}

static void on_menu(int i) {
    if (i >= 1 && i <= 4) { mode = MODE_TIME; time_i = i - 1; }
    else if (i >= 6 && i <= 9) { mode = MODE_WORDS; count_i = i - 6; }
    else if (i == 12 || i == 13) lang = i - 12;
    else if (i == 15 || i == 16) big = i - 15;
    else if (i == 18) punct = !punct;
    else if (i == 19) nums = !nums;
    else if (i == 20) lazy = !lazy;
    else if (i >= 22 && i <= 24) diff = i - 22;
    else return;
    save_cfg();
    restart();
}

static void focus(int f) { focused = f; widget_redraw(); }
static void hide(int h) { if (h && state == RUN) restart(); }

static void init(void) {
    load_cfg();
    cw = gfx_text_width(L"m", 1, FONT, TEXT_MONO);
    if (cw < 1) cw = 8;
    cols = (W - 2 * PAD) / cw;
    restart();
}

int main(void) {
    WidgetDesc d = {
        .name = L"teclear", .w = W, .h = H, .fps = 0,
        .on_init = init, .on_tick = tick, .on_draw = draw, .on_char = on_char,
        .on_hide = hide, .on_focus = focus,
        .menu = MENU, .menu_checked = menu_checked, .on_menu = on_menu,
    };
    return widget_run(&d);
}
