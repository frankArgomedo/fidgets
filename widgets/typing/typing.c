/*
 * typing - test de mecanografia: escribir palabras al azar contra el reloj o por
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
 *
 * Ficha de tamano (ver "Tamano y espacio" en CLAUDE.md):
 *   Requisitos: 3 lineas de texto de al menos 30 caracteres (se lee la siguiente
 *     linea mientras se escribe; con menos de ~30 las palabras saltan de linea seguido).
 *   Estados persistentes: reposo, escribiendo (incl. letras de mas), resultado, fallo.
 *     Transitorio: el cursor deslizandose.
 *   Papeles: texto del test, cuenta regresiva y datos del resultado = principal
 *     (15/14/15 px); encabezado, avisos y "wpm" = secundario (12-13 px); WPM y "fallo"
 *     = destacado (56 / 44 px).
 *   Celda: 280x120. Minimo: 3 lineas a 14 px con interlineado 1,5x + encabezado no
 *     entran en 80 de alto; 30 caracteres de Consolas a 14-15 px (8 px c/u) + margenes
 *     no entran en 240 de ancho. De lo que cabe, 280x120 es la de menor area. La letra
 *     no pasa de 15 px porque a 16 px (9 px por caracter) quedarian 29 caracteres;
 *     el alto que sobra (< 40 px) se reparte con interlineado 1,67x y centrando.
 */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include "widget.h"

#pragma clang diagnostic ignored "-Wc23-extensions"

#define W 280          /* cuadricula de 40 px: 7x3 modulos (ver ficha) */
#define H 120
#define PAD 8
#define FONT 15          /* texto del test, monoespaciado (principal, tope 16) */
#define TOP 33           /* y de la primera linea: bloque centrado bajo el encabezado */
#define LH 25            /* alto de linea: 1,67x la letra (rango 1,5x-1,7x) */
#define LINES 3
#ifndef ANIM_FPS
#define ANIM_FPS 45      /* mientras el cursor se desliza */
#endif

/* ------------------------------------------------------------ listas de palabras
 * Los .txt (UTF-8, una por linea, orden de frecuencia) van dentro del .exe con
 * #embed. La lista "200" son las primeras 200 lineas de la de 1000. */
static const char ES_TXT[] = {
#embed "words/es_1000.txt"
    , 0 };
static const char EN_TXT[] = {
#embed "words/en_1000.txt"
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
    cur_pb = cfg_get_range(k, 0, 0, 1000000);
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
        gfx_text(PAD, TOP, L"falló", theme->error, 44, ALIGN_LEFT | TEXT_SHADOW);
        gfx_text(PAD, TOP + 56, fail_reason == 1 ? L"dejaste una palabra con error" : L"tecla incorrecta",
                 theme->sub, 15, ALIGN_LEFT | TEXT_SHADOW);
        return;
    }
    fmt2(b, res_wpm);
    gfx_text(PAD, TOP - 4, b, theme->accent, 56, ALIGN_LEFT | TEXT_SHADOW);
    int x = PAD + gfx_text_width(b, -1, 56, 0) + 4;
    gfx_text(x, TOP + 43, L"wpm", theme->sub, 13, ALIGN_LEFT | TEXT_SHADOW);
    int cx = 140;              /* deja lugar a un WPM de 3 cifras */
    wsprintfW(b, L"precisión %d%%", (res_acc + 50) / 100);
    gfx_text(cx, TOP - 2, b, theme->text, 15, ALIGN_LEFT | TEXT_SHADOW);
    wsprintfW(b, L"bruto %d", (res_raw + 50) / 100);
    gfx_text(cx, TOP + 24, b, theme->sub, 15, ALIGN_LEFT | TEXT_SHADOW);
    Color c = theme->sub;
    if (res_low_acc) { lstrcpyW(b, L"no cuenta: < 75%"); c = theme->error; }
    else if (res_new_pb) { lstrcpyW(b, L"¡nuevo récord!"); c = theme->accent; }
    else if (res_pb > 0) wsprintfW(b, L"récord %d", (res_pb + 50) / 100);
    else b[0] = 0;
    if (b[0]) gfx_text(cx, TOP + 50, b, c, 15, ALIGN_LEFT | TEXT_SHADOW);
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

/* Textos de la interfaz (ver «Idiomas y textos» en CLAUDE.md). */
enum {
    T_NONE, T_TITLE, T_MODE, T_TIME_15, T_TIME_30, T_TIME_60, T_TIME_120,
    T_WORDS_10, T_WORDS_25, T_WORDS_50, T_WORDS_100,
    T_PUNCT, T_NUMS, T_NO_ACCENTS, T_WORDLIST, T_SPANISH, T_ENGLISH, T_LIST_200, T_LIST_1000,
    T_DIFF, T_DIFF_0, T_DIFF_1, T_DIFF_2, T_CLEAR, T_CLEAR_YES,
    T_H_CLICK, T_H_SPACE, T_H_BACK, T_H_CTRL_BACK, T_H_TAB, T_H_ESC, T_COUNT
};
static const wchar_t *const TXT_ES[T_COUNT] = {
    [T_TITLE] = L"Teclear",
    [T_MODE] = L"Modo",
    [T_TIME_15] = L"Tiempo: 15 s", [T_TIME_30] = L"Tiempo: 30 s",
    [T_TIME_60] = L"Tiempo: 60 s", [T_TIME_120] = L"Tiempo: 120 s",
    [T_WORDS_10] = L"Palabras: 10", [T_WORDS_25] = L"Palabras: 25",
    [T_WORDS_50] = L"Palabras: 50", [T_WORDS_100] = L"Palabras: 100",
    [T_PUNCT] = L"Puntuación", [T_NUMS] = L"Números", [T_NO_ACCENTS] = L"Sin tildes",
    [T_WORDLIST] = L"Palabras",
    [T_SPANISH] = L"En español", [T_ENGLISH] = L"En inglés",
    [T_LIST_200] = L"Las 200 más comunes", [T_LIST_1000] = L"Las 1000 más comunes",
    [T_DIFF] = L"Dificultad",
    [T_DIFF_0] = L"Normal",
    [T_DIFF_1] = L"Experto: falla si dejas una palabra mal",
    [T_DIFF_2] = L"Maestro: falla con el primer error",
    [T_CLEAR] = L"Borrar récords", [T_CLEAR_YES] = L"Sí, borrar todos",
    [T_H_CLICK] = L"Clic · darle el teclado",
    [T_H_SPACE] = L"Espacio · siguiente palabra",
    [T_H_BACK] = L"Retroceso · corregir",
    [T_H_CTRL_BACK] = L"Ctrl+Retroceso · borrar la palabra",
    [T_H_TAB] = L"Tab · palabras nuevas",
    [T_H_ESC] = L"Esc · cancelar el test",
};
static const wchar_t *const TXT_EN[T_COUNT] = {
    [T_TITLE] = L"Typing",
    [T_MODE] = L"Mode",
    [T_TIME_15] = L"Time: 15 s", [T_TIME_30] = L"Time: 30 s",
    [T_TIME_60] = L"Time: 60 s", [T_TIME_120] = L"Time: 120 s",
    [T_WORDS_10] = L"Words: 10", [T_WORDS_25] = L"Words: 25",
    [T_WORDS_50] = L"Words: 50", [T_WORDS_100] = L"Words: 100",
    [T_PUNCT] = L"Punctuation", [T_NUMS] = L"Numbers", [T_NO_ACCENTS] = L"No accents",
    [T_WORDLIST] = L"Words",
    [T_SPANISH] = L"In Spanish", [T_ENGLISH] = L"In English",
    [T_LIST_200] = L"Top 200", [T_LIST_1000] = L"Top 1000",
    [T_DIFF] = L"Difficulty",
    [T_DIFF_0] = L"Normal",
    [T_DIFF_1] = L"Expert: fail if you leave a word wrong",
    [T_DIFF_2] = L"Master: fail on the first mistake",
    [T_CLEAR] = L"Clear records", [T_CLEAR_YES] = L"Yes, clear them all",
    [T_H_CLICK] = L"Click · give it the keyboard",
    [T_H_SPACE] = L"Space · next word",
    [T_H_BACK] = L"Backspace · correct",
    [T_H_CTRL_BACK] = L"Ctrl+Backspace · delete the word",
    [T_H_TAB] = L"Tab · new words",
    [T_H_ESC] = L"Esc · cancel the test",
};

static const int LISTS[2] = { 200, 1000 };
enum {
    M_MODE, M_TIME_15, M_TIME_30, M_TIME_60, M_TIME_120, M_MODE_SEP,
    M_WORDS_10, M_WORDS_25, M_WORDS_50, M_WORDS_100, M_MODE_END,
    M_PUNCT, M_NUMS, M_NO_ACCENTS,
    M_WORDLIST, M_SPANISH, M_ENGLISH, M_LIST_SEP, M_LIST_200, M_LIST_1000, M_WORDLIST_END,
    M_DIFF, M_DIFF_0, M_DIFF_1, M_DIFF_2, M_DIFF_END,
    M_SEP, M_CLEAR, M_CLEAR_YES, M_CLEAR_END, M_COUNT
};
static const int MENU[M_COUNT + 1] = {
    [M_MODE] = T_MODE | MENU_SUB,
    [M_TIME_15] = T_TIME_15, [M_TIME_30] = T_TIME_30, [M_TIME_60] = T_TIME_60, [M_TIME_120] = T_TIME_120,
    [M_MODE_SEP] = MENU_SEP,
    [M_WORDS_10] = T_WORDS_10, [M_WORDS_25] = T_WORDS_25, [M_WORDS_50] = T_WORDS_50, [M_WORDS_100] = T_WORDS_100,
    [M_MODE_END] = MENU_END,
    [M_PUNCT] = T_PUNCT, [M_NUMS] = T_NUMS, [M_NO_ACCENTS] = T_NO_ACCENTS,
    [M_WORDLIST] = T_WORDLIST | MENU_SUB,
    [M_SPANISH] = T_SPANISH, [M_ENGLISH] = T_ENGLISH, [M_LIST_SEP] = MENU_SEP,
    [M_LIST_200] = T_LIST_200, [M_LIST_1000] = T_LIST_1000, [M_WORDLIST_END] = MENU_END,
    [M_DIFF] = T_DIFF | MENU_SUB,
    [M_DIFF_0] = T_DIFF_0, [M_DIFF_1] = T_DIFF_1, [M_DIFF_2] = T_DIFF_2, [M_DIFF_END] = MENU_END,
    [M_SEP] = MENU_SEP,
    [M_CLEAR] = T_CLEAR | MENU_SUB, [M_CLEAR_YES] = T_CLEAR_YES, [M_CLEAR_END] = MENU_END,
};
static const int HELP[] = { T_H_CLICK, T_H_SPACE, T_H_BACK, T_H_CTRL_BACK, T_H_TAB, T_H_ESC, 0 };

static int radio(int on) { return on ? CHECK_RADIO : CHECK_NONE; }

static int menu_checked(int i) {
    if (i >= M_TIME_15 && i <= M_TIME_120) return radio(mode == MODE_TIME && time_i == i - M_TIME_15);
    if (i >= M_WORDS_10 && i <= M_WORDS_100) return radio(mode == MODE_WORDS && count_i == i - M_WORDS_10);
    if (i == M_SPANISH || i == M_ENGLISH) return radio(lang == i - M_SPANISH);
    if (i == M_LIST_200 || i == M_LIST_1000) return radio(big == i - M_LIST_200);
    if (i >= M_DIFF_0 && i <= M_DIFF_2) return radio(diff == i - M_DIFF_0);
    if (i == M_PUNCT) return punct;
    if (i == M_NUMS) return nums;
    if (i == M_NO_ACCENTS) return lazy;
    return CHECK_NONE;
}

/* Se guarda el valor real (segundos, palabras, tamano de la lista), no la posicion
 * en el menu. */
static void save_cfg(void) {
    cfg_set_int(L"mode", mode);
    cfg_set_int(L"time", TIMES[time_i]);
    cfg_set_int(L"words", COUNTS[count_i]);
    cfg_set_int(L"lang", lang);
    cfg_set_int(L"list", big ? 1000 : 200);
    cfg_set_int(L"punctuation", punct);
    cfg_set_int(L"numbers", nums);
    cfg_set_int(L"no_accents", lazy);
    cfg_set_int(L"difficulty", diff);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* posicion de v en a[0..n-1]; def si no esta */
static int index_of(const int *a, int n, int v, int def) {
    for (int i = 0; i < n; i++) if (a[i] == v) return i;
    return def;
}

/* TEMPORAL (quitar antes de publicar): claves del nombre provisional "teclear".
 * tiempo, palabras y lista guardaban la posicion en el menu; ahora el valor real. */
static void migrate_cfg(void) {
    if (cfg_rename(L"tiempo", L"time")) cfg_set_int(L"time", TIMES[clampi(cfg_get_int(L"time", 1), 0, 3)]);
    if (cfg_rename(L"palabras", L"words")) cfg_set_int(L"words", COUNTS[clampi(cfg_get_int(L"words", 1), 0, 3)]);
    if (cfg_rename(L"lista", L"list")) cfg_set_int(L"list", cfg_get_int(L"list", 0) ? 1000 : 200);
    cfg_rename(L"modo", L"mode");
    cfg_rename(L"idioma", L"lang");
    cfg_rename(L"puntuacion", L"punctuation");
    cfg_rename(L"numeros", L"numbers");
    cfg_rename(L"sintildes", L"no_accents");
    cfg_rename(L"dificultad", L"difficulty");
}

static void load_cfg(void) {
    migrate_cfg();
    mode = cfg_get_range(L"mode", MODE_TIME, 0, 1);
    time_i = index_of(TIMES, 4, cfg_get_choice(L"time", 30, TIMES, 4), 1);
    count_i = index_of(COUNTS, 4, cfg_get_choice(L"words", 25, COUNTS, 4), 1);
    lang = cfg_get_range(L"lang", 0, 0, 1);
    big = cfg_get_choice(L"list", 200, LISTS, 2) == 1000;
    punct = cfg_get_range(L"punctuation", 0, 0, 1);
    nums = cfg_get_range(L"numbers", 0, 0, 1);
    lazy = cfg_get_range(L"no_accents", 0, 0, 1);
    diff = cfg_get_range(L"difficulty", 0, 0, 2);
}

static void on_menu(int i) {
    if (i >= M_TIME_15 && i <= M_TIME_120) { mode = MODE_TIME; time_i = i - M_TIME_15; }
    else if (i >= M_WORDS_10 && i <= M_WORDS_100) { mode = MODE_WORDS; count_i = i - M_WORDS_10; }
    else if (i == M_SPANISH || i == M_ENGLISH) lang = i - M_SPANISH;
    else if (i == M_LIST_200 || i == M_LIST_1000) big = i - M_LIST_200;
    else if (i == M_PUNCT) punct = !punct;
    else if (i == M_NUMS) nums = !nums;
    else if (i == M_NO_ACCENTS) lazy = !lazy;
    else if (i >= M_DIFF_0 && i <= M_DIFF_2) diff = i - M_DIFF_0;
    else if (i == M_CLEAR_YES) { cfg_delete_prefix(L"pb_"); restart(); return; }
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
        .name = L"typing", .w = W, .h = H, .fps = 0,
        .on_init = init, .on_tick = tick, .on_draw = draw, .on_char = on_char,
        .on_hide = hide, .on_focus = focus,
        .texts = { TXT_ES, TXT_EN }, .ntexts = T_COUNT,
        .menu = MENU, .menu_checked = menu_checked, .on_menu = on_menu, .help = HELP,
        .legacy_ini = L"teclear",
    };
    return widget_run(&d);
}
