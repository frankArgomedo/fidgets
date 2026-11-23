/*
 * teclas - mapa de teclas del cubo virtual. Es un "acompanante" de cubo: vive
 * dentro de cubo.exe (se abre con "cubo.exe --teclas") y lo abre y cierra el
 * cubo desde su menu. No es un programa aparte.
 *
 * Cada tecla muestra arriba su etiqueta real en la distribucion de teclado que
 * este usando Windows (en espanol se ve "Ñ", en ingles ";") y abajo el giro que
 * hace. Cuando "cubo" recibe una tecla le avisa a este widget y la tecla se
 * ilumina un instante. En reposo no dibuja nada (0% CPU); solo anima los
 * 250 ms del destello.
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
#define W TECLAS_W
#define H TECLAS_H

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
            flash[r][c] = 1;
            update_names();
            widget_set_fps(30);
            widget_redraw();
        }
    }
}

static void message(int code, int value) {
    if (code == MSG_TECLA_USADA) light(value);
    else if (code == MSG_CERRAR_MAPA) widget_close();
}

/* si el usuario lo cierra a mano, el cubo apaga la opcion "Mapa de teclas" */
static void closed(int by_user) { if (by_user) widget_post(L"cubo", MSG_MAPA_CERRADO, 0); }

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

static Color mix(Color a, Color b, float t) {
    int r = (int)(((a >> 16) & 255) * (1 - t) + ((b >> 16) & 255) * t);
    int g = (int)(((a >> 8) & 255) * (1 - t) + ((b >> 8) & 255) * t);
    int bl = (int)((a & 255) * (1 - t) + (b & 255) * t);
    int al = (int)((a >> 24) * (1 - t) + (b >> 24) * t);
    return RGBA(r, g, bl, al);
}

static void draw(void) {
    gfx_clear_hit();
    update_names();
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 10; c++) {
            int scan = ROW_SCAN[r] + c, x = OX + ROW_OFF[r] + c * STRIDE, y = OY + r * ROWH;
            const Key *k = key_for(scan);
            Color cap = !k ? RGBA(40, 42, 48, 90) : (k->rot ? RGBA(55, 75, 135, 225) : RGBA(52, 55, 64, 230));
            if (flash[r][c] > 0) cap = mix(cap, RGBA(70, 210, 130, 250), flash[r][c]);
            gfx_round_rect(x, y, CELL, CAPH, 3, cap);
            gfx_text(x + 2, y, names[r][c], k ? theme->sub : RGBA(175, 180, 192, 120), 8, ALIGN_LEFT);
            if (k) {
                wchar_t lb[3] = { (wchar_t)k->label[0], (wchar_t)k->label[1], 0 };
                gfx_text(x + CELL / 2, y + 9, lb, theme->text, 10, ALIGN_CENTER);
            }
        }
}

static void init(void) { update_names(); }

int teclas_main(void) {
    WidgetDesc d = {
        .name = L"teclas", .w = W, .h = H, .fps = 0,
        .on_init = init, .on_tick = tick, .on_draw = draw, .on_key = key, .on_message = message,
        .on_close = closed,
    };
    return widget_run(&d);
}
