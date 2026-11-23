/*
 * widget.h - base comun para los widgets de "bored".
 *
 * Cada widget es un .exe diminuto: ventana sin bordes, con transparencia por
 * pixel, que se puede arrastrar, fijar encima de todo y ocultar/cerrar rapido.
 *
 * Uso minimo:
 *
 *   #include "widget.h"
 *   static void draw(void) { gfx_clear(RGBA(20,20,20,200)); gfx_text(...); }
 *   int main(void) {
 *       WidgetDesc d = { .name = L"demo", .w = 160, .h = 100, .on_draw = draw };
 *       return widget_run(&d);
 *   }
 *
 * Coordenadas: siempre en pixeles LOGICOS (96 dpi). La base escala sola.
 * Colores: RGBA(r,g,b,a) normal; la base los premultiplica internamente.
 *
 * Atajos comunes (los da la base, no hay que programarlos):
 *   Arrastrar con clic izquierdo  -> mover (si el widget no consume el clic)
 *   Ctrl + arrastrar              -> mover siempre
 *   Clic derecho                  -> menu (encima de todo, opacidad, ocultar, cerrar)
 *   Clic central / Esc            -> cerrar este widget (Esc solo si on_key no lo consume)
 *   Ctrl+Alt+B                    -> ocultar / mostrar TODOS los widgets
 *   Ctrl+Alt+Q                    -> cerrar TODOS los widgets
 */
#ifndef BORED_WIDGET_H
#define BORED_WIDGET_H

#include <stdint.h>

typedef uint32_t Color; /* 0xAARRGGBB, sin premultiplicar */
#define RGBA(r, g, b, a) ((Color)(((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b)))

enum { MOUSE_DOWN = 1, MOUSE_UP = 2 };
enum { ALIGN_LEFT = 0, ALIGN_CENTER = 1, ALIGN_RIGHT = 2 };
#define TEXT_SHADOW 4  /* combinar con la alineacion: sombra oscura de 1 px para fondos claros */

typedef struct WidgetDesc {
    const wchar_t *name;  /* identificador unico: seccion del .ini y mutex */
    int w, h;             /* tamano logico (recomendado <= 300x300) */
    int fps;              /* 0 = solo redibuja cuando se pide (CPU ~0%) */

    void (*on_init)(void);
    void (*on_tick)(float dt);     /* solo si fps > 0 y la ventana es visible */
    void (*on_draw)(void);         /* dibujar con gfx_* */
    /* vk = tecla virtual (depende del idioma del teclado), scan = codigo fisico
     * (misma posicion en cualquier distribucion), down: 1 pulsada, 2 autorepeticion,
     * 0 soltada. Devuelve 1 si la consumio (p.ej. Esc deja de cerrar el widget). */
    int  (*on_key)(int vk, int scan, int down);
    /* devuelve 1 si consumio el clic; 0 deja que la base arrastre la ventana */
    int  (*on_mouse)(int x, int y, int ev);
    void (*on_hide)(int hidden);   /* aviso al ocultar/mostrar (p.ej. pausar) */
    void (*on_focus)(int focused); /* el widget gano/perdio el teclado */
    void (*on_message)(int code, int value); /* mensaje de otro widget (widget_post) */
    void (*on_close)(int by_user);   /* al cerrarse; by_user = 1 si lo cerro el usuario a mano */

    /* Opciones propias en el menu del clic derecho (lista terminada en NULL). */
    const wchar_t *const *menu;
    int  (*menu_checked)(int i);   /* 1 = mostrar marca; puede ser NULL */
    void (*on_menu)(int i);
} WidgetDesc;

int  widget_run(const WidgetDesc *d);
void widget_redraw(void);          /* pide un redibujado */
void widget_set_fps(int fps);      /* 0 detiene el bucle de animacion */
void widget_close(void);
int  widget_mouse(int *x, int *y); /* posicion actual; devuelve 1 si esta dentro */
/* Envia (code, value) al widget 'name' si esta abierto; no bloquea. 1 si existe. */
int  widget_post(const wchar_t *name, int code, int value);
/* posicion y tamano de la ventana en pixeles de pantalla; 0 si aun no hay ventana */
int  widget_rect(int *x, int *y, int *w, int *h);

/* Configuracion persistente (%APPDATA%\bored\<exe>.ini, seccion = name). */
int  cfg_get_int(const wchar_t *key, int def);
void cfg_set_int(const wchar_t *key, int value);
/* lo mismo para la seccion de otro widget del mismo programa */
int  cfg_get_int_in(const wchar_t *section, const wchar_t *key, int def);
void cfg_set_int_in(const wchar_t *section, const wchar_t *key, int value);

/* Dibujo sobre el buffer de la ventana. */
void gfx_clear(Color c);
/* fondo invisible que igual recibe clics (para arrastrar sin panel visible),
 * aun con la opacidad mas baja */
void gfx_clear_hit(void);
void gfx_rect(int x, int y, int w, int h, Color c);
void gfx_round_rect(int x, int y, int w, int h, int r, Color c);
void gfx_circle(float cx, float cy, float r, Color c);
void gfx_poly(const float *xy, int n, Color c); /* poligono CONVEXO, xy = x0,y0,x1,y1... */
void gfx_text(int x, int y, const wchar_t *s, Color c, int size, int align);

/* Utilidades. */
uint32_t rnd(void);                /* xorshift, ya sembrado */
int      rnd_range(int lo, int hi);/* [lo, hi] */
double   now_seconds(void);

#endif
