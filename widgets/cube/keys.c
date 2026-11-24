/*
 * keys - mapa de teclas del cubo virtual. Es un «acompañante» de cube: vive dentro
 * de cube.exe (se abre con "cube.exe --keys"), lo abre y cierra el cubo desde su
 * menú y se cierra con él. No es un programa aparte.
 *
 * Cada tecla muestra arriba su etiqueta real en la distribución de teclado que esté
 * usando Windows (en español se ve «Ñ», en inglés «;») y abajo el giro que hace.
 * Cuando cube recibe una tecla le avisa a este widget y la tecla se ilumina un
 * instante. En reposo no dibuja nada (0 % CPU); solo anima los 250 ms del destello.
 *
 * Ficha (ver CLAUDE.md):
 *   Tipo: otro (solo se mira).
 *   Requisitos: las 40 teclas en su posición real; en cada una, su letra y su giro.
 *   Estados persistentes: reposo. Transitorio: tecla iluminada (250 ms; con
 *     animaciones desactivadas no se ilumina). El destello no cambia tamaños.
 *   Papeles: teclas = gráfico; letra de la tecla (8 px) y giro (10 px) = EXCEPCIÓN.
 *   Celda: 240x120. Mínimo: teclas de 20x24 (lo justo para el giro a 10 px) ->
 *     8+15+9*21+20+8 = 240 de ancho y 7+3*27+24+8 = 120 de alto; ninguna celda menor.
 *   Al ocultar / sin foco / al cerrar: nada (no tiene actividad); al cerrarlo a mano
 *     el cubo apaga su opción «Mapa de teclas».
 *   Aviso para empezar: —
 *   Controles: — (sin controles propios)
 *   Récords: —
 *   Claves del .ini: sección [keys] en cube.ini, solo las de la base (posición,
 *     opacidad…).
 *   Mensajes: recibe MSG_KEY_USED (scan code) y MSG_CLOSE_MAP de cube; envía
 *     MSG_MAP_CLOSED a cube.
 *   Colores del juego: —  (teclas: surface; rotaciones: surface_alt; destello: ok)
 *   Sonido: —
 *   Origen: mapa de teclas de csTimer (keymap.h).
 *   Excepciones: letras de 8 px y giros de 10 px (con 11 px las 40 teclas no entran
 *     en 240 px); las teclas sin giro van tenues (decorativas).
 */
#include "widget.h"
#include "keymap.h"
#include <windows.h>

#define CELL 20          /* ancho de una tecla */
#define CAPH 24          /* alto de una tecla */
#define STRIDE 21
#define ROWH 27
#define OX 8             /* 8 + 15 + 9*21 + 20 + 8 = 240 (>= MARGIN) */
#define OY 7             /* 7 + 3*27 + 24 + 8 = 120 */
#define W KEYS_W
#define H KEYS_H

/* filas del teclado: primer scan code de la fila y desplazamiento como en un teclado real */
static const unsigned char ROW_SCAN[4] = { 0x02, 0x10, 0x1E, 0x2C };
static const unsigned char ROW_OFF[4] = { 0, 5, 9, 15 };

static float flash[4][10];
static wchar_t names[4][10][3];
static HKL names_hkl;

/* etiquetas segun la distribucion activa (la de la ventana en primer plano) */
static void update_names(void) {
    HWND fg = GetForegroundWindow();
    HKL hkl = GetKeyboardLayout(fg ? GetWindowThreadProcessId(fg, NULL) : 0);
    if (hkl == names_hkl && names[0][0][0]) return;
    names_hkl = hkl;
    BYTE ks[256] = { 0 };
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 10; c++) {
            UINT scan = ROW_SCAN[r] + c, vk = MapVirtualKeyExW(scan, 1 /* VSC_TO_VK */, hkl);
            wchar_t b[4] = { 0 };
            int n = ToUnicodeEx(vk, scan, ks, b, 4, 4 /* no altera el estado de teclas muertas */, hkl);
            names[r][c][0] = n != 0 ? b[0] : L'?';
            names[r][c][1] = 0;
            CharUpperW(names[r][c]);
        }
}

static void light(int scan) {
    for (int r = 0; r < 4; r++) {
        int c = scan - ROW_SCAN[r];
        if (c >= 0 && c < 10) {
            if (widget_reduce_motion()) return;   /* transitorio: salta a su final */
            flash[r][c] = 1;
            update_names();
            widget_set_fps(FPS_EFFECT);
            widget_redraw();
        }
    }
}

static void message(int code, int value) {
    if (code == MSG_KEY_USED) light(value);
    else if (code == MSG_CLOSE_MAP) widget_close();
}

/* si el usuario lo cierra a mano, el cubo apaga la opcion "Mapa de teclas" */
static void closed(int by_user) { if (by_user) widget_post(L"cube", MSG_MAP_CLOSED, 0); }

/* tambien se ilumina si se escribe con este widget seleccionado */
static int key(int vk, int scan, int down) { (void)vk; if (down == 1) light(scan); return 0; }

static void tick(float dt) {
    int any = 0;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 10; c++)
            if (flash[r][c] > 0) {
                flash[r][c] -= dt * 4;   /* 250 ms */
                if (flash[r][c] < 0) flash[r][c] = 0;
                any |= flash[r][c] > 0;
            }
    if (!any) widget_set_fps(0);
}

static void draw(void) {
    gfx_clear_hit();
    update_names();
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 10; c++) {
            int scan = ROW_SCAN[r] + c, x = OX + ROW_OFF[r] + c * STRIDE, y = OY + r * ROWH;
            const Key *k = key_for(scan);
            Color cap = !k ? color_alpha(theme->surface, 90) : (k->rot ? theme->surface_alt : theme->surface);
            if (flash[r][c] > 0) cap = color_mix(cap, theme->ok, flash[r][c]);
            gfx_round_rect(x, y, CELL, CAPH, 3, cap);
            gfx_text(x + 2, y, names[r][c], k ? theme->sub : color_alpha(theme->sub, 110), 8, ALIGN_LEFT | TEXT_SHADOW);
            if (k) {
                wchar_t lb[3] = { (wchar_t)k->label[0], (wchar_t)k->label[1], 0 };
                gfx_text(x + CELL / 2, y + 9, lb, theme->text, 10, ALIGN_CENTER | TEXT_SHADOW);
            }
        }
}

static void init(void) { update_names(); }

enum { T_NONE, T_TITLE, T_COUNT };
static const wchar_t *const TXT_ES[T_COUNT] = { [T_TITLE] = L"Mapa de teclas" };
static const wchar_t *const TXT_EN[T_COUNT] = { [T_TITLE] = L"Key map" };

int keys_main(void) {
    WidgetDesc d = {
        .name = L"keys", .w = W, .h = H, .fps = 0,
        .on_init = init, .on_tick = tick, .on_draw = draw, .on_key = key, .on_message = message,
        .on_close = closed,
        .texts = { TXT_ES, TXT_EN }, .ntexts = T_COUNT,
        .legacy_ini = L"cubo", .legacy_section = L"teclas",
    };
    return widget_run(&d);
}
