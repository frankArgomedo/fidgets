/*
 * punto - minijuego de reflejos.
 * Aparece un punto que se encoge; hazle clic antes de que desaparezca.
 * Fallar el clic o dejar que se esfume cuesta una vida (3 vidas).
 * En reposo no anima nada (fps 0); solo corre a 30 fps mientras juegas.
 */
#include "widget.h"
#include <windows.h>

#define W 160
#define H 120
#define TOP 22 /* franja superior para el marcador */

enum { IDLE, PLAY };
static int state = IDLE, score, best, lives;
static float dx, dy, life, life_max, flash;

static void new_dot(void) {
    life_max = 1.6f - score * 0.03f;
    if (life_max < 0.55f) life_max = 0.55f;
    life = life_max;
    dx = (float)rnd_range(14, W - 14);
    dy = (float)rnd_range(TOP + 12, H - 12);
}

static void lose_life(void) {
    flash = 0.25f;
    if (--lives <= 0) {
        state = IDLE;
        if (score > best) { best = score; cfg_set_int(L"best", best); }
        widget_set_fps(0);
    } else new_dot();
}

static void init(void) { best = cfg_get_int(L"best", 0); }

static void tick(float dt) {
    if (flash > 0) flash -= dt;
    if (state != PLAY) { if (flash <= 0) widget_set_fps(0); return; }
    life -= dt;
    if (life <= 0) lose_life();
}

static float dot_r(void) { return 4 + 9 * (life / life_max); }

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
    gfx_clear(0);
    gfx_round_rect(0, 0, W, H, 12, RGBA(16, 18, 24, 175));
    if (flash > 0) gfx_round_rect(0, 0, W, H, 12, RGBA(220, 60, 60, (int)(flash * 300)));

    if (state == IDLE) {
        wsprintfW(buf, L"Mejor: %d", best);
        gfx_text(W / 2, 8, buf, RGBA(200, 205, 215, 200), 12, ALIGN_CENTER);
        gfx_circle(W / 2.0f, H / 2.0f + 8, 20, RGBA(90, 200, 140, 220));
        /* triangulo "play" hecho con rectangulos finos */
        for (int i = 0; i < 12; i++)
            gfx_rect(W / 2 - 5 + i, H / 2 + 8 - (12 - i) / 2, 1, 12 - i, RGBA(16, 18, 24, 230));
        return;
    }
    wsprintfW(buf, L"%d", score);
    gfx_text(10, 4, buf, RGBA(230, 232, 240, 230), 13, ALIGN_LEFT);
    for (int i = 0; i < lives; i++) gfx_circle(W - 12 - i * 11, 12, 3.5f, RGBA(230, 90, 90, 220));
    gfx_rect(8, TOP, W - 16, 1, RGBA(255, 255, 255, 25));
    float t = life / life_max;
    gfx_circle(dx, dy, dot_r(), RGBA((int)(255 - 120 * t), (int)(90 + 130 * t), 120, 235));
}

static void hide(int hidden) { (void)hidden; /* el temporizador se pausa solo */ }

int main(void) {
    WidgetDesc d = {
        .name = L"punto", .w = W, .h = H, .fps = 0,
        .on_init = init, .on_tick = tick, .on_draw = draw,
        .on_mouse = mouse, .on_hide = hide,
    };
    return widget_run(&d);
}
