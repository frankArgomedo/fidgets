/*
 * widget.h - base común para los widgets de "fidgets". Reglas: ver CLAUDE.md.
 *
 * Cada widget es un .exe diminuto: ventana sin bordes, con transparencia por
 * píxel, que se puede arrastrar, fijar encima de todo y ocultar/cerrar rápido.
 *
 * Uso mínimo:
 *
 *   #include "widget.h"
 *   enum { T_NONE, T_HELLO, T_COUNT };
 *   static const wchar_t *const TXT_ES[T_COUNT] = { [T_HELLO] = L"hola" };
 *   static const wchar_t *const TXT_EN[T_COUNT] = { [T_HELLO] = L"hello" };
 *   static void draw(void) { gfx_clear_hit(); gfx_text(8, 8, tr(T_HELLO), theme->text, 14, TEXT_SHADOW); }
 *   int main(void) {
 *       WidgetDesc d = { .name = L"demo", .w = 160, .h = 120, .on_draw = draw,
 *                        .texts = { TXT_ES, TXT_EN }, .ntexts = T_COUNT };
 *       return widget_run(&d);
 *   }
 *
 * Coordenadas: siempre en píxeles LÓGICOS (96 dpi). La base escala sola.
 * Colores: RGBA(r,g,b,a) normal; la base los premultiplica internamente.
 *
 * Lo que da la base (no hay que programarlo):
 *   Arrastrar con clic izquierdo  -> mover (si el widget no consume el clic)
 *   Ctrl + arrastrar              -> mover siempre
 *   (al mover hay imán: se pega a los bordes y a otros widgets, sin separación; Shift lo evita)
 *   Clic derecho                  -> menú (controles, encima de todo, opacidad, idioma, ocultar, cerrar)
 *   Clic central                  -> cerrar este widget (Esc NO cierra: queda libre para el widget)
 *   Ctrl+Alt+B                    -> ocultar / mostrar TODOS los widgets
 *   Ctrl+Alt+Q                    -> cerrar TODOS los widgets
 *   Una sola instancia por name; on_hide también al suspender la PC o bloquear la sesión.
 *
 * Pruebas sin ventana (modo foto): FIDGETS_SNAPSHOT=archivo dibuja una vez y sale.
 * FIDGETS_EVENTS es un guion de eventos en orden, separados por espacios:
 *   key:17,24   scan codes en hex (pulsar; vk se deduce)  text:hola\sque   texto para on_char
 *   up:17,24    soltar esas teclas (para las que se mantienen pulsadas)
 *   click:80,68 clic (bajar y subir) en x,y       tick:500         ms simulados
 *   hide / show / focus / blur                    lang:es | lang:en
 * En text: \s espacio, \b retroceso, \t Tab, \n Enter; cada carácter avanza 100 ms
 * el reloj (now_seconds usa un reloj simulado en este modo) y da el foco si no lo
 * tenía. FIDGETS_SEED=n fija el azar.
 */
#ifndef FIDGETS_WIDGET_H
#define FIDGETS_WIDGET_H

#include <stdint.h>

typedef uint32_t Color; /* 0xAARRGGBB, sin premultiplicar */
#define RGBA(r, g, b, a) ((Color)(((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b)))

enum { MOUSE_DOWN = 1, MOUSE_UP = 2 };
enum { ALIGN_LEFT = 0, ALIGN_CENTER = 1, ALIGN_RIGHT = 2 };
#define TEXT_SHADOW 4  /* combinar con la alineación: sombra oscura de 1 px para fondos claros */
#define TEXT_MONO 8    /* combinar: fuente monoespaciada (Consolas) en vez de Segoe UI */

/* Margen OBLIGATORIO entre el borde del widget y su contenido visible. El imán
 * pega los widgets sin separación, así que este margen es el que los separa. */
#define MARGIN 6

/* Fps mientras algo se mueve: nunca un número suelto en widget_set_fps (salvo 0). */
#define FPS_MOVE 45      /* movimiento que la vista sigue (giros, deslizamientos) */
#define FPS_EFFECT 30    /* efectos: cambio de tamaño o color, destellos */
#define FPS_CLOCK 20     /* cronómetro con centésimas corriendo */
#define FPS_COUNTER 10   /* contadores en segundos (cada número sale <= 100 ms tarde) */

/* Teclas extendidas (flechas, Insert, Supr, Inicio, Fin, RePág/AvPág, Enter numérico,
 * Ctrl y Alt derechos): on_key recibe el scan code con este bit sumado. */
#define SCAN_EXTENDED 0x100

/* Paleta común: la interfaz usa solo estos colores (base para temas futuros).
 * Los colores "del juego" (p.ej. las caras del cubo) van como constantes del widget. */
typedef struct Theme {
    Color text;        /* texto principal */
    Color sub;         /* texto secundario / pendiente */
    Color accent;      /* acento: cursor, cuenta regresiva, récord, avisos */
    Color error;       /* errores */
    Color extra;       /* errores secundarios (p.ej. letras de más) */
    Color surface;     /* relleno de teclas y controles */
    Color surface_alt; /* segunda categoría de control (p.ej. tecla de rotación) */
    Color line;        /* separadores y bordes (decorativo) */
    Color ok;          /* acierto, éxito, «jugar» */
    Color ink;         /* ícono o texto encima de un relleno de color */
} Theme;
extern const Theme *theme;
Color color_mix(Color a, Color b, float t);  /* t = 0 -> a, 1 -> b (incluye el alfa) */
Color color_alpha(Color c, int a);           /* el mismo color con otro alfa (0-255) */

/* Idiomas de la interfaz. Cada widget da una tabla de textos por idioma, indexada por
 * sus ids T_… (el 0 queda libre: T_NONE). Si a un idioma le falta un texto, se usa el
 * español. */
enum { LANG_ES, LANG_EN, LANG_COUNT };
const wchar_t *tr(int id);       /* texto del widget en el idioma actual */
int widget_lang(void);           /* LANG_ES / LANG_EN */
/* textos comunes de la base, para avisos que todos los widgets muestran igual */
enum { TB_PAUSED, TB_NO_KEYBOARD, TB_COUNT };
const wchar_t *tr_base(int id);

/* Menú propio: arreglo de ids de texto, terminado en 0. Con | MENU_SUB la entrada
 * abre un submenú con ese título; MENU_END lo cierra; MENU_SEP es un separador.
 * Usar inicializadores designados: [M_INSPECTION] = T_INSPECTION, … */
#define MENU_SUB 0x10000
#define MENU_END (-1)
#define MENU_SEP (-2)
/* menu_checked: 0 nada, 1 = ✓ (se activa y desactiva), 2 = • (una de varias) */
enum { CHECK_NONE, CHECK_ON, CHECK_RADIO };

typedef struct WidgetDesc {
    const wchar_t *name;  /* nombre propio (inglés): .exe, sección del .ini, mutex */
    int w, h;             /* tamaño lógico: múltiplos de 40, de 80 a 320 */
    int fps;              /* 0 = solo redibuja cuando se pide (CPU ~0%) */

    void (*on_init)(void);
    void (*on_tick)(float dt);     /* solo si fps > 0 y el widget se puede usar */
    void (*on_draw)(void);         /* dibujar con gfx_* */
    /* vk = tecla virtual (depende del idioma del teclado), scan = código físico
     * (misma posición en cualquier distribución; + SCAN_EXTENDED si es extendida),
     * down: 1 pulsada, 2 autorrepetición, 0 soltada. Devuelve 1 si la consumió. */
    int  (*on_key)(int vk, int scan, int down);
    /* carácter ya compuesto según el teclado del usuario (WM_CHAR: tildes, ñ, mayúsculas;
     * también 8 = retroceso, 9 = Tab, 13 = Enter, 127 = Ctrl+retroceso). Se llama después
     * de on_key. Devuelve 1 si lo consumió. */
    int  (*on_char)(int ch);
    /* devuelve 1 si consumió el clic; 0 deja que la base arrastre la ventana */
    int  (*on_mouse)(int x, int y, int ev);
    /* 1 = el widget no se puede usar (oculto, PC suspendida o sesión bloqueada);
     * 0 = vuelve a poder usarse. Ver «Ciclo de vida» en CLAUDE.md. */
    void (*on_hide)(int hidden);
    void (*on_focus)(int focused); /* el widget ganó/perdió el teclado */
    void (*on_message)(int code, int value); /* mensaje de otro widget (widget_post) */
    void (*on_close)(int by_user);   /* al cerrarse; by_user = 1 si lo cerró el usuario a mano */

    /* textos de la interfaz: texts[LANG_ES] obligatoria; ntexts = cantidad de ids */
    const wchar_t *const *texts[LANG_COUNT];
    int ntexts;

    const int *menu;               /* ver MENU_SUB; on_menu / menu_checked reciben el índice */
    int  (*menu_checked)(int i);   /* CHECK_…; puede ser NULL */
    void (*on_menu)(int i);
    const int *help;               /* «Controles ▸»: ids de texto, terminado en 0 */
} WidgetDesc;

int  widget_run(const WidgetDesc *d);
void widget_redraw(void);          /* pide un redibujado */
void widget_set_fps(int fps);      /* FPS_… o 0 (detiene el bucle de animación) */
void widget_close(void);
int  widget_mouse(int *x, int *y); /* posición actual; devuelve 1 si está dentro */
/* Envía (code, value) al widget 'name' si está abierto; no bloquea. 1 si existe. */
int  widget_post(const wchar_t *name, int code, int value);
/* posición y tamaño de la ventana en píxeles de pantalla; 0 si aún no hay ventana */
int  widget_rect(int *x, int *y, int *w, int *h);
/* 1 si Windows tiene desactivadas las animaciones: los efectos transitorios saltan
 * a su final (la mecánica del juego no cambia) */
int  widget_reduce_motion(void);

/* Configuración persistente (%APPDATA%\fidgets\<exe>.ini, sección = name).
 * Leer siempre validando (cfg_get_range / cfg_get_choice); guardar el valor real. */
int  cfg_get_int(const wchar_t *key, int def);
int  cfg_get_range(const wchar_t *key, int def, int min, int max);       /* def si sale del rango */
int  cfg_get_choice(const wchar_t *key, int def, const int *vals, int n); /* def si no es uno de vals */
void cfg_set_int(const wchar_t *key, int value);
void cfg_delete_prefix(const wchar_t *prefix);   /* borra las claves que empiezan con prefix */
/* lo mismo para la sección de otro widget del mismo programa */
int  cfg_get_int_in(const wchar_t *section, const wchar_t *key, int def);
void cfg_set_int_in(const wchar_t *section, const wchar_t *key, int value);

/* Dibujo sobre el buffer de la ventana. */
void gfx_clear(Color c);
/* fondo invisible que igual recibe clics (para arrastrar sin panel visible),
 * aun con la opacidad más baja */
void gfx_clear_hit(void);
void gfx_rect(int x, int y, int w, int h, Color c);
void gfx_round_rect(int x, int y, int w, int h, int r, Color c);
void gfx_circle(float cx, float cy, float r, Color c);
void gfx_poly(const float *xy, int n, Color c); /* polígono CONVEXO, xy = x0,y0,x1,y1... */
void gfx_text(int x, int y, const wchar_t *s, Color c, int size, int align);
/* ancho lógico de los primeros n caracteres de s (n < 0 = todo); flags: TEXT_MONO */
int  gfx_text_width(const wchar_t *s, int n, int size, int flags);
/* texto que se parte en dos líneas (en " · " o en un espacio) si no cabe a lo ancho
 * del widget dentro del margen; interlineado 1,5x. Devuelve las líneas (1 o 2). */
int  gfx_text_lines(const wchar_t *s, int size, int flags);
int  gfx_text_wrap(int x, int y, const wchar_t *s, Color c, int size, int align);

/* Utilidades. */
uint32_t rnd(void);                /* xorshift, ya sembrado (FIDGETS_SEED lo fija) */
int      rnd_range(int lo, int hi);/* [lo, hi] */
double   now_seconds(void);        /* reloj monótono; nunca la hora del sistema */

#endif
