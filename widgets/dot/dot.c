/*
 * dot - minijuego de reflejos: aparece un punto que se encoge; hazle clic antes de
 * que desaparezca. Fallar el clic o dejar que se esfume cuesta una vida (3 vidas).
 *
 * Ficha (ver CLAUDE.md):
 *   Tipo: juego.
 *   Requisitos: el punto (hasta 26 px) puede aparecer en al menos 5 posiciones
 *     distintas a lo ancho y 3 a lo alto, para que haya que mover el mouse; marcador
 *     y vidas mientras se juega; puntaje final y récord al terminar.
 *   Estados persistentes: reposo (botón), jugando, en pausa, resultado.
 *     Transitorio: destello de fallo (250 ms; con animaciones desactivadas no se ve).
 *   Papeles: marcador = principal (14 px); «Mejor», «¡nuevo récord!» y la pausa =
 *     secundario (12 px); puntaje final = destacado (32 px); botón y área de juego =
 *     gráfico / zona para clicar (botón de 40 px).
 *   Celda: 160x120. Mínimo: 5x26 + márgenes = 142 de ancho -> 160; marcador 18 + 3x26
 *     + márgenes = 108 de alto -> 120. El área de juego ocupa toda el área útil.
 *   Al ocultar / sin foco / al cerrar: al ocultar se pausa y al volver espera un clic;
 *     sin foco no cambia nada (se juega con el mouse); al cerrar se pierde la partida
 *     (dura menos de un minuto).
 *   Aviso para empezar: el botón de jugar (se queda en el resultado para repetir).
 *   Controles: ver HELP.
 *   Récords: el mayor puntaje («best»); ningún ajuste lo separa.
 *   Claves del .ini: best = mayor puntaje (0-1000000; 0 = sin récord).
 *   Mensajes: —
 *   Colores del juego: —  (el punto va de theme->error a theme->ok según su vida)
 *   Sonido: —
 *   Origen: mecánica clásica de juego de reflejos; aspecto propio.
 *   Excepciones: el punto se achica hasta 8 px de diámetro (es la mecánica del juego).
 */
#include "widget.h"
#include <windows.h>

#define W 160
#define H 120
#define TOP 24      /* franja superior para el marcador */
#define R_MAX 13    /* radio máximo del punto */
#define BTN_R 20    /* radio del botón de jugar */

enum { T_NONE, T_TITLE, T_BEST, T_NO_RECORD, T_NEW_RECORD, T_CLEAR, T_CLEAR_YES, T_H_PLAY, T_H_DOT, T_H_MISS, T_COUNT };
static const wchar_t *const TXT_ES[T_COUNT] = {
    [T_TITLE] = L"Punto",
    [T_BEST] = L"Mejor: %s",
    [T_NO_RECORD] = L"—",
    [T_NEW_RECORD] = L"¡nuevo récord!",
    [T_CLEAR] = L"Borrar récord", [T_CLEAR_YES] = L"Sí, borrarlo",
    [T_H_PLAY] = L"Clic en el botón · jugar",
    [T_H_DOT] = L"Clic en el punto · un punto más",
    [T_H_MISS] = L"Fallar o dejar que se esfume · una vida menos",
};
static const wchar_t *const TXT_EN[T_COUNT] = {
    [T_TITLE] = L"Dot",
    [T_BEST] = L"Best: %s",
    [T_NEW_RECORD] = L"new record!",
    [T_CLEAR] = L"Clear record", [T_CLEAR_YES] = L"Yes, clear it",
    [T_H_PLAY] = L"Click the button · play",
    [T_H_DOT] = L"Click the dot · one more point",
    [T_H_MISS] = L"Miss it or let it fade · one life less",
};

enum { IDLE, PLAY, RESULT };
static int state = IDLE, score, best, lives, paused, new_record;
static float dx, dy, life, life_max, flash;

/* el botón: al centro en reposo; a la derecha en el resultado (el puntaje va a la izquierda) */
static float btn_x(void) { return state == RESULT ? W - MARGIN - 34.0f : W / 2.0f; }
static float btn_y(void) { return H / 2.0f + 8; }

static void new_dot(void) {
    life_max = 1.6f - score * 0.03f;
    if (life_max < 0.55f) life_max = 0.55f;
    life = life_max;
    dx = (float)rnd_range(MARGIN + R_MAX, W - MARGIN - R_MAX);
    dy = (float)rnd_range(TOP + R_MAX, H - MARGIN - R_MAX);
}

static void lose_life(void) {
    flash = widget_reduce_motion() ? 0 : 0.25f;
    if (--lives <= 0) {
        state = RESULT;
        new_record = score > best;
        if (new_record) { best = score; cfg_set_int(L"best", best); }
        if (flash <= 0) widget_set_fps(0);
    } else new_dot();
}

static void init(void) { best = cfg_get_range(L"best", 0, 0, 1000000); }

static void tick(float dt) {
    if (flash > 0) flash -= dt;
    if (state != PLAY || paused) { if (flash <= 0) widget_set_fps(0); return; }
    life -= dt;
    if (life <= 0) lose_life();
}

static float dot_r(void) { return 4 + (R_MAX - 4) * (life / life_max); }

static int mouse(int x, int y, int ev) {
    if (ev != MOUSE_DOWN) return state == PLAY;
    if (state == PLAY && paused) {          /* el clic solo saca de la pausa */
        paused = 0;
        widget_set_fps(FPS_EFFECT);
        widget_redraw();
        return 1;
    }
    if (state != PLAY) {
        /* solo el botón inicia; el resto del fondo arrastra la ventana */
        float ddx = x - btn_x(), ddy = y - btn_y();
        if (ddx * ddx + ddy * ddy > BTN_R * BTN_R) return 0;
        state = PLAY; score = 0; lives = 3; flash = 0; new_record = 0;
        new_dot();
        widget_set_fps(FPS_EFFECT);
        widget_redraw();
        return 1;
    }
    float ddx = x - dx, ddy = y - dy, r = dot_r() + 3; /* un poco de margen */
    if (ddx * ddx + ddy * ddy <= r * r) { score++; new_dot(); }
    else lose_life();
    widget_redraw();
    return 1;
}

static void play_button(void) {
    float cx = btn_x(), cy = btn_y();
    gfx_circle(cx, cy, BTN_R, theme->ok);
    /* triángulo «jugar» hecho con rectángulos finos */
    for (int i = 0; i < 12; i++)
        gfx_rect((int)cx - 5 + i, (int)cy - (12 - i) / 2, 1, 12 - i, theme->ink);
}

static void draw(void) {
    wchar_t buf[48], num[16];
    gfx_clear_hit();   /* sin panel: fondo invisible que igual se puede arrastrar */
    if (flash > 0)     /* fallo: un destello rojo suave dentro del margen */
        gfx_round_rect(MARGIN, MARGIN, W - 2 * MARGIN, H - 2 * MARGIN, 10, color_alpha(theme->error, (int)(flash * 280)));

    if (state != PLAY) {
        if (state == RESULT && new_record)
            gfx_text(W / 2, MARGIN + 2, tr(T_NEW_RECORD), theme->accent, 12, ALIGN_CENTER | TEXT_SHADOW);
        else {
            if (best > 0) wsprintfW(num, L"%d", best); else lstrcpyW(num, tr(T_NO_RECORD));
            wsprintfW(buf, tr(T_BEST), num);
            gfx_text(W / 2, MARGIN + 2, buf, theme->sub, 12, ALIGN_CENTER | TEXT_SHADOW);
        }
        if (state == RESULT) {          /* puntaje final: dato destacado; en accent si es récord */
            wsprintfW(buf, L"%d", score);
            gfx_text(MARGIN + 44, (int)btn_y() - 22, buf, new_record ? theme->accent : theme->text, 32,
                     ALIGN_CENTER | TEXT_SHADOW);
        }
        play_button();
        return;
    }
    wsprintfW(buf, L"%d", score);
    gfx_text(MARGIN + 2, MARGIN, buf, theme->text, 14, ALIGN_LEFT | TEXT_SHADOW);
    for (int i = 0; i < lives; i++) gfx_circle(W - MARGIN - 6 - i * 11, MARGIN + 8, 3.5f, theme->error);
    gfx_rect(MARGIN, TOP - 2, W - 2 * MARGIN, 1, theme->line);
    if (paused) {
        gfx_text_wrap(W / 2, H / 2 - 6, tr_base(TB_PAUSED), theme->accent, 12, ALIGN_CENTER | TEXT_SHADOW);
        return;
    }
    gfx_circle(dx, dy, dot_r(), color_mix(theme->error, theme->ok, life / life_max));
}

/* juego: al dejar de poder usarse se pausa; al volver espera un clic */
static void hide(int hidden) {
    if (hidden && state == PLAY) { paused = 1; widget_set_fps(0); flash = 0; }
    widget_redraw();
}

enum { M_CLEAR, M_CLEAR_YES, M_CLEAR_END, M_COUNT };
static const int MENU[M_COUNT + 1] = { [M_CLEAR] = T_CLEAR | MENU_SUB, [M_CLEAR_YES] = T_CLEAR_YES, [M_CLEAR_END] = MENU_END };
static const int HELP[] = { T_H_PLAY, T_H_DOT, T_H_MISS, 0 };

static void on_menu(int i) {
    if (i == M_CLEAR_YES) { best = 0; new_record = 0; cfg_set_int(L"best", 0); }
}

int main(void) {
    WidgetDesc d = {
        .name = L"dot", .w = W, .h = H, .fps = 0,
        .on_init = init, .on_tick = tick, .on_draw = draw,
        .on_mouse = mouse, .on_hide = hide,
        .texts = { TXT_ES, TXT_EN }, .ntexts = T_COUNT,
        .menu = MENU, .on_menu = on_menu, .help = HELP,
    };
    return widget_run(&d);
}
