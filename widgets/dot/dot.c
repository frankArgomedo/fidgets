/*
 * dot - minijuego de reflejos.
 * Aparece un punto que se encoge; hazle clic antes de que desaparezca.
 * Fallar el clic o dejar que se esfume cuesta una vida (3 vidas).
 * En reposo no anima nada (fps 0); solo corre a 30 fps mientras juegas.
 *
 * Ficha de tamano (ver "Tamano y espacio" en CLAUDE.md):
 *   Requisitos: el punto (hasta 26 px) puede aparecer en al menos 5 posiciones
 *     distintas a lo ancho y 3 a lo alto, para que haya que mover el mouse; marcador.
 *   Estados persistentes: reposo (boton) y jugando; transitorio: destello de fallo.
 *   Papeles: marcador = principal (14 px); "Mejor" = secundario (12 px); boton y
 *     area de juego = grafico/zona para clicar; el punto que se achica hasta 8 px es
 *     EXCEPCION (mecanica del juego).
 *   Celda: 160x120. Minimo: 5x26 + margenes = 142 de ancho -> 160; marcador 18 + 3x26
 *     + margenes = 108 de alto -> 120. El area de juego ocupa toda el area util.
 */
#include "widget.h"
#include <windows.h>

#define W 160
#define H 120
#define TOP 24 /* franja superior para el marcador */
#define R_MAX 13 /* radio maximo del punto */

enum { IDLE, PLAY };
static int state = IDLE, score, best, lives;
static float dx, dy, life, life_max, flash;

static void new_dot(void) {
    life_max = 1.6f - score * 0.03f;
    if (life_max < 0.55f) life_max = 0.55f;
    life = life_max;
    dx = (float)rnd_range(MARGIN + R_MAX, W - MARGIN - R_MAX);
    dy = (float)rnd_range(TOP + R_MAX, H - MARGIN - R_MAX);
}

static void lose_life(void) {
    flash = 0.25f;
    if (--lives <= 0) {
        state = IDLE;
        if (score > best) { best = score; cfg_set_int(L"best", best); }
        widget_set_fps(0);
    } else new_dot();
}

static void init(void) { best = cfg_get_range(L"best", 0, 0, 1000000); }

static void tick(float dt) {
    if (flash > 0) flash -= dt;
    if (state != PLAY) { if (flash <= 0) widget_set_fps(0); return; }
    life -= dt;
    if (life <= 0) lose_life();
}

static float dot_r(void) { return 4 + (R_MAX - 4) * (life / life_max); }

static int mouse(int x, int y, int ev) {
    if (ev != MOUSE_DOWN) return state == PLAY;
    if (state == IDLE) {
        /* solo el boton central inicia; el resto del fondo arrastra la ventana */
        float ddx = x - W / 2.0f, ddy = y - (H / 2.0f + 8);
        if (ddx * ddx + ddy * ddy > 20 * 20) return 0;
        state = PLAY; score = 0; lives = 3; flash = 0;
        new_dot();
        widget_set_fps(30);
        widget_redraw();
        return 1;
    }
    float ddx = x - dx, ddy = y - dy, r = dot_r() + 3; /* un poco de margen */
    if (ddx * ddx + ddy * ddy <= r * r) { score++; new_dot(); }
    else lose_life();
    widget_redraw();
    return 1;
}

static void draw(void) {
    wchar_t buf[32];
    gfx_clear_hit();   /* sin panel: fondo invisible que igual se puede arrastrar */
    if (flash > 0) {    /* fallo: un destello rojo suave dentro del margen */
        Color e = theme->error;
        gfx_round_rect(MARGIN, MARGIN, W - 2 * MARGIN, H - 2 * MARGIN, 10, (e & 0xFFFFFF) | ((Color)(int)(flash * 280) << 24));
    }

    if (state == IDLE) {
        wsprintfW(buf, L"Mejor: %d", best);
        gfx_text(W / 2, MARGIN + 2, buf, theme->sub, 12, ALIGN_CENTER | TEXT_SHADOW);
        gfx_circle(W / 2.0f, H / 2.0f + 8, 20, RGBA(90, 200, 140, 220));
        /* triangulo "play" hecho con rectangulos finos */
        for (int i = 0; i < 12; i++)
            gfx_rect(W / 2 - 5 + i, H / 2 + 8 - (12 - i) / 2, 1, 12 - i, RGBA(16, 18, 24, 230));
        return;
    }
    wsprintfW(buf, L"%d", score);
    gfx_text(MARGIN + 2, MARGIN, buf, theme->text, 14, ALIGN_LEFT | TEXT_SHADOW);
    for (int i = 0; i < lives; i++) gfx_circle(W - MARGIN - 6 - i * 11, MARGIN + 8, 3.5f, theme->error);
    gfx_rect(MARGIN, TOP - 2, W - 2 * MARGIN, 1, RGBA(255, 255, 255, 40));
    float t = life / life_max;
    gfx_circle(dx, dy, dot_r(), RGBA((int)(255 - 120 * t), (int)(90 + 130 * t), 120, 235));
}

static void hide(int hidden) { (void)hidden; /* el temporizador se pausa solo */ }

/* Textos de la interfaz (ver «Idiomas y textos» en CLAUDE.md). */
enum { T_NONE, T_TITLE, T_CLEAR, T_CLEAR_YES, T_H_PLAY, T_H_DOT, T_H_MISS, T_COUNT };
static const wchar_t *const TXT_ES[T_COUNT] = {
    [T_TITLE] = L"Punto",
    [T_CLEAR] = L"Borrar récord", [T_CLEAR_YES] = L"Sí, borrarlo",
    [T_H_PLAY] = L"Clic en el botón · jugar",
    [T_H_DOT] = L"Clic en el punto · un punto más",
    [T_H_MISS] = L"Fallar o dejar que se esfume · una vida menos",
};
static const wchar_t *const TXT_EN[T_COUNT] = {
    [T_TITLE] = L"Dot",
    [T_CLEAR] = L"Clear record", [T_CLEAR_YES] = L"Yes, clear it",
    [T_H_PLAY] = L"Click the button · play",
    [T_H_DOT] = L"Click the dot · one more point",
    [T_H_MISS] = L"Miss it or let it fade · one life less",
};
enum { M_CLEAR, M_CLEAR_YES, M_CLEAR_END, M_COUNT };
static const int MENU[M_COUNT + 1] = { [M_CLEAR] = T_CLEAR | MENU_SUB, [M_CLEAR_YES] = T_CLEAR_YES, [M_CLEAR_END] = MENU_END };
static const int HELP[] = { T_H_PLAY, T_H_DOT, T_H_MISS, 0 };

static void on_menu(int i) {
    if (i == M_CLEAR_YES) { best = 0; cfg_set_int(L"best", 0); }
}

int main(void) {
    WidgetDesc d = {
        .name = L"dot", .w = W, .h = H, .fps = 0,
        .on_init = init, .on_tick = tick, .on_draw = draw,
        .on_mouse = mouse, .on_hide = hide,
        .texts = { TXT_ES, TXT_EN }, .ntexts = T_COUNT,
        .menu = MENU, .on_menu = on_menu, .help = HELP,
        .legacy_ini = L"punto",
    };
    return widget_run(&d);
}
