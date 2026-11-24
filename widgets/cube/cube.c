/*
 * cube - timer de speedcubing con cubo virtual 3x3 manejado con el teclado.
 *
 * Flujo del timer como csTimer (modo "virtual"): Espacio mezcla (y, si está activa,
 * empieza la inspección de 15 s); el primer giro que no sea rotación arranca el
 * tiempo; al resolver se detiene. Esc aborta (con el tiempo corriendo es DNF).
 * Las teclas se leen por POSICIÓN física (scan code), así el mapa es el mismo con
 * teclado español/latino que con uno en inglés (la tecla de la ñ es ";").
 *
 * Modelo: 54 pegatinas con posición y normal enteras; cada giro rota 90 grados
 * las que están en las capas afectadas. Sirve igual para caras, capas
 * centrales (M E S), giros dobles (r l u d) y rotaciones (x y z).
 *
 * Ficha (ver CLAUDE.md):
 *   Tipo: medición.
 *   Requisitos: tiempo legible y el cubo al menos del tamaño validado (escala 225:
 *     pegatinas frontales de ~27 px, caras laterales distinguibles con opacidad baja).
 *   Estados persistentes: reposo (con o sin resultado), inspección, listo y tiempo
 *     corriendo, con cualquier cámara (13 x 13 ángulos); cada uno con o sin teclado.
 *     Transitorios: los giros (100 ms por defecto; instantáneos con animaciones
 *     desactivadas).
 *   Papeles: tiempo = destacado (28 px); avisos = secundario (12 px); cubo = gráfico.
 *   Celda: 160x200. Mínimo: cubo de 126x134 en el peor ángulo + tiempo -> no entra en
 *     120 de ancho ni en 160 de alto. El cubo se agranda (escala 261) hasta que en el
 *     ángulo más ancho llega a 7 px del borde.
 *   Al ocultar / sin foco / al cerrar: al ocultar (o suspender, o bloquear) se cancela
 *     la resolución sin penalizar; sin teclado el tiempo sigue, con el aviso «sin
 *     teclado · clic para seguir»; al cerrar se pierde la resolución en curso.
 *   Aviso para empezar: «espacio: mezclar», hasta el primer resultado.
 *   Controles: ver HELP.
 *   Récords: el mejor tiempo («best»); ningún ajuste lo separa. Un DNF no cuenta.
 *     Récord nuevo: el tiempo en accent. Solo se muestran el tiempo y el cubo
 *     (preferencia del usuario); las últimas 12 se guardan aunque no se dibujen.
 *   Claves del .ini: best = mejor tiempo en ms (0 = sin récord); t0…t11 = últimos
 *     tiempos en ms (-1 DNF, -2 vacío); inspection (0-1); turn_ms (0, 50, 100, 200);
 *     cam_t, cam_p (-6..6, pasos de 7,5°); map = mapa de teclas abierto (0-1).
 *     Sección [keys]: posición y opacidad del mapa (las pone la base).
 *   Mensajes: envía MSG_KEY_USED y MSG_CLOSE_MAP a keys; recibe MSG_MAP_CLOSED.
 *   Colores del juego: FACE_COL = colores de las caras (los de csTimer, estándar de
 *     un cubo real); BODY_COL = borde negro de las pegatinas.
 *   Sonido: —
 *   Origen: mapa de teclas, flujo del timer, cámara y colores de csTimer (GPL-3.0;
 *     se tomaron la mecánica y las convenciones, no código). Solver de dos fases de
 *     Kociemba: implementación propia (twophase.c).
 *   Excepciones: memoria privada hasta 7 MB (tope común 2,5 MB): las tablas del
 *     solver de dos fases ocupan ~3,7 MB y se arman al primer «mezclar».
 */
#include "widget.h"
#include "twophase.h"
#include <windows.h>
#include <string.h>
#include <math.h>

#define W 160
#define H 200   /* cuadrícula de 40 px: 4x5 módulos */
#define CX 80.0f
#define CY 118.0f  /* el cubo llena el ancho y queda debajo del tiempo */

/* Textos de la interfaz (ver «Idiomas y textos» en CLAUDE.md). */
enum {
    T_NONE, T_TITLE, T_INSPECTION, T_KEYMAP, T_SPEED, T_SPEED_0, T_SPEED_50, T_SPEED_100, T_SPEED_200,
    T_CLEAR, T_CLEAR_YES, T_H_SPACE, T_H_KEYS, T_H_ARROWS, T_H_ESC,
    T_DNF, T_PLUS2, T_HINT_START, T_CLICK_KEYBOARD, T_COUNT
};
static const wchar_t *const TXT_ES[T_COUNT] = {
    [T_TITLE] = L"Cubo",
    [T_INSPECTION] = L"Inspección 15 s",
    [T_KEYMAP] = L"Mapa de teclas",
    [T_SPEED] = L"Velocidad de giro",
    [T_SPEED_0] = L"Instantánea",
    [T_SPEED_50] = L"Rápida (50 ms)",
    [T_SPEED_100] = L"Normal (100 ms)",
    [T_SPEED_200] = L"Lenta (200 ms)",
    [T_CLEAR] = L"Borrar tiempos y récord",
    [T_CLEAR_YES] = L"Sí, borrar todo",
    [T_H_SPACE] = L"Espacio · mezclar",
    [T_H_KEYS] = L"Letras · girar (ver Mapa de teclas)",
    [T_H_ARROWS] = L"Flechas · girar la cámara",
    [T_H_ESC] = L"Esc · abortar (con el tiempo corriendo: DNF)",
    [T_DNF] = L"DNF",
    [T_PLUS2] = L"+2",
    [T_HINT_START] = L"espacio: mezclar",
    [T_CLICK_KEYBOARD] = L"clic para usar el teclado",
};
static const wchar_t *const TXT_EN[T_COUNT] = {
    [T_TITLE] = L"Cube",
    [T_INSPECTION] = L"15 s inspection",
    [T_KEYMAP] = L"Key map",
    [T_SPEED] = L"Turn speed",
    [T_SPEED_0] = L"Instant",
    [T_SPEED_50] = L"Fast (50 ms)",
    [T_SPEED_100] = L"Normal (100 ms)",
    [T_SPEED_200] = L"Slow (200 ms)",
    [T_CLEAR] = L"Clear times and record",
    [T_CLEAR_YES] = L"Yes, clear everything",
    [T_H_SPACE] = L"Space · scramble",
    [T_H_KEYS] = L"Letters · turn (see Key map)",
    [T_H_ARROWS] = L"Arrows · rotate the camera",
    [T_H_ESC] = L"Esc · abort (while timing: DNF)",
    [T_HINT_START] = L"space: scramble",
    [T_CLICK_KEYBOARD] = L"click to use the keyboard",
};

/* aspecto (se pueden cambiar al compilar con -D para probar variantes) */
#ifndef STICKER_HALF
#define STICKER_HALF 0.435f /* media pegatina: 0.435 = 87% de la celda (csTimer usa 90%); deja ver mas las caras ocultas */
#endif
#ifndef BORDER
#define BORDER 0.03f       /* grosor del borde negro */
#endif

/* ------------------------------------------------------------ modelo */

typedef struct { signed char p[3], n[3]; unsigned char col; } Sticker;
static Sticker st[54];

/* Colores del juego (no siguen al tema). Caras como csTimer: U blanco, R rojo,
 * F verde, D amarillo, L naranja, B azul; borde negro de las pegatinas. */
static const Color FACE_COL[6] = {
    RGBA(255, 255, 255, 255), RGBA(255, 0, 0, 255), RGBA(0, 255, 0, 255),
    RGBA(255, 255, 0, 255), RGBA(255, 144, 0, 255), RGBA(0, 0, 255, 255),
};
#define BODY_COL RGBA(0, 0, 0, 255)

static int face_index(const signed char *n) {
    if (n[1] > 0) return 0;
    if (n[0] > 0) return 1;
    if (n[2] > 0) return 2;
    if (n[1] < 0) return 3;
    if (n[0] < 0) return 4;
    return 5;
}

static void reset_cube(void) {
    int k = 0;
    for (int ax = 0; ax < 3; ax++)
        for (int s = -1; s <= 1; s += 2)
            for (int u = -1; u <= 1; u++)
                for (int v = -1; v <= 1; v++) {
                    Sticker *t = &st[k++];
                    t->p[ax] = (signed char)s;
                    t->p[(ax + 1) % 3] = (signed char)u;
                    t->p[(ax + 2) % 3] = (signed char)v;
                    t->n[0] = t->n[1] = t->n[2] = 0;
                    t->n[ax] = (signed char)s;
                    t->col = (unsigned char)face_index(t->n);
                }
}

/* rota +90 grados (regla de la mano derecha) alrededor del eje a */
static void rot90(signed char *v, int a) {
    signed char x = v[0], y = v[1], z = v[2];
    if (a == 0) { v[1] = (signed char)-z; v[2] = y; }
    else if (a == 1) { v[0] = z; v[2] = (signed char)-x; }
    else { v[0] = (signed char)-y; v[1] = x; }
}

/* ax: 0=x 1=y 2=z; capas con lo <= coord <= hi; q = cuartos de vuelta (+90) */
static void move_on(Sticker *cube, int ax, int lo, int hi, int q) {
    for (int i = 0; i < 54; i++) {
        Sticker *t = &cube[i];
        if (t->p[ax] < lo || t->p[ax] > hi) continue;
        for (int k = 0; k < q; k++) { rot90(t->p, ax); rot90(t->n, ax); }
    }
}

static void do_move(int ax, int lo, int hi, int q) { move_on(st, ax, lo, hi, q); }

/* ------------------------------------------------------------ animacion
 * Como csTimer: el estado logico (st) cambia al instante con cada tecla, asi el
 * tiempo y la deteccion de resuelto son exactos. El estado visual (vis) va
 * detras: cada giro entra a una cola y se anima durante turn_ms. Si la cola se
 * llena, la velocidad sube x(en_cola + 2) / 2. Giros del mismo eje seguidos
 * (R y L, U y D...) se animan a la vez. */
typedef struct { signed char ax, lo, hi, q; } Move;
static Sticker vis[54];
static Move queue[64], active[8];
static float prog[8];
static int nq, nact, turn_ms = 100;

static void sync_vis(void) { memcpy(vis, st, sizeof vis); nq = nact = 0; }
static int animating(void) { return nact > 0 || nq > 0; }

static void start_next(void) {
    active[nact] = queue[0];
    prog[nact++] = 0;
    memmove(queue, queue + 1, (size_t)(--nq) * sizeof queue[0]);
}

static void anim_push(int ax, int lo, int hi, int q) {
    Move m = { (signed char)ax, (signed char)lo, (signed char)hi, (signed char)q };
    if (turn_ms <= 0 || nq == 64 || widget_reduce_motion()) {   /* instantáneo o cola llena: aplicar todo ya */
        for (int i = 0; i < nact; i++) move_on(vis, active[i].ax, active[i].lo, active[i].hi, active[i].q);
        for (int i = 0; i < nq; i++) move_on(vis, queue[i].ax, queue[i].lo, queue[i].hi, queue[i].q);
        nq = nact = 0;
        move_on(vis, ax, lo, hi, q);
        return;
    }
    queue[nq++] = m;
    if (nact == 0 || (nact < 8 && queue[0].ax == active[0].ax)) start_next();
}

static void anim_tick(float dt) {
    if (!nact) return;
    float step = dt * 1000.0f / (float)turn_ms * (float)(nq + 2) / 2.0f;
    if (step > 1) step = 1;
    for (int i = 0; i < nact; i++) prog[i] += step;
    while (nact && prog[0] >= 1) {               /* el primero termino: fijarlo en vis */
        move_on(vis, active[0].ax, active[0].lo, active[0].hi, active[0].q);
        memmove(active, active + 1, (size_t)(nact - 1) * sizeof active[0]);
        memmove(prog, prog + 1, (size_t)(nact - 1) * sizeof prog[0]);
        nact--;
    }
    if (nact == 0 && nq > 0) start_next();
    while (nq > 0 && nact > 0 && nact < 8 && queue[0].ax == active[0].ax) start_next();
}

/* angulo (radianes) que lleva girada la pegatina por las animaciones activas */
static float anim_angle(const Sticker *t, int *axis) {
    float a = 0;
    for (int i = 0; i < nact; i++) {
        const Move *m = &active[i];
        if (t->p[m->ax] < m->lo || t->p[m->ax] > m->hi) continue;
        *axis = m->ax;
        a += (m->q == 3 ? -1.0f : (float)m->q) * prog[i] * 1.5707963f;
    }
    return a;
}

static int is_solved(void) {
    int c[6] = { -1, -1, -1, -1, -1, -1 };
    for (int i = 0; i < 54; i++) {
        int f = face_index(st[i].n);
        if (c[f] < 0) c[f] = st[i].col;
        else if (c[f] != st[i].col) return 0;
    }
    return 1;
}

#include "keymap.h"

/* Mezcla random-state (como la WCA); si las tablas no se pudieran crear, se
 * usan 25 giros aleatorios sin redundancias. */
static unsigned char scr[32];
static int scr_len;

static void scramble(void) {
    /* caras en el orden del solver: U R F D L B -> eje, capa, q del giro horario */
    static const signed char F[6][3] = {
        { 1, 1, 3 }, { 0, 1, 3 }, { 2, 1, 3 }, { 1, -1, 1 }, { 0, -1, 1 }, { 2, -1, 1 },
    };
    reset_cube();
    scr_len = tp_random_scramble(rnd, scr, (int)sizeof scr);
    if (scr_len > 0) {
        for (int i = 0; i < scr_len; i++) {
            int f = scr[i] / 3, turns = scr[i] % 3 + 1;
            do_move(F[f][0], F[f][1], F[f][1], (F[f][2] * turns) % 4);
        }
        if (!is_solved()) return;
    }
    reset_cube();
    int last = -1, prev = -1;
    for (int i = 0; i < 25; i++) {
        int f;
        do {
            f = rnd_range(0, 5);
        } while (f == last || (last >= 0 && prev >= 0 && f % 3 == last % 3 && f % 3 == prev % 3));
        int turns = rnd_range(1, 3); /* 1: horario, 2: doble, 3: antihorario */
        do_move(F[f][0], F[f][1], F[f][1], (F[f][2] * turns) % 4);
        prev = last;
        last = f;
    }
    if (is_solved()) scramble();
}

/* ------------------------------------------------------------ timer */

static int cam_t, cam_p;
static void setup_camera(void);

enum { IDLE, INSPECT, READY, RUN };
static int state = IDLE, use_ins, focused;
static double t0;
static int penalty;          /* 0, 2000 o -1 (DNF) */
static int moves, last_ms = -2, last_pen, last_moves, last_cs; /* last_ms -2: sin tiempo */
static int last_best;        /* el último tiempo fue récord nuevo */
static int times[12], ntimes, best;

static void load_stats(void) {
    wchar_t k[8];
    ntimes = 0;
    for (int i = 0; i < 12; i++) {
        wsprintfW(k, L"t%d", i);
        int v = cfg_get_range(k, -2, -2, 36000000);
        if (v == -2) break;
        times[ntimes++] = v;
    }
    best = cfg_get_range(L"best", 0, 0, 36000000);
    use_ins = cfg_get_range(L"inspection", 0, 0, 1);
}

static void save_stats(void) {
    wchar_t k[8];
    for (int i = 0; i < 12; i++) {
        wsprintfW(k, L"t%d", i);
        cfg_set_int(k, i < ntimes ? times[i] : -2);
    }
    cfg_set_int(L"best", best);
}

static void record(int ms) {
    for (int i = 11; i > 0; i--) times[i] = times[i - 1];
    times[0] = ms;
    if (ntimes < 12) ntimes++;
    last_best = ms >= 0 && (best == 0 || ms < best);
    if (last_best) best = ms;
    save_stats();
}

/* media de n quitando el mejor y el peor; -1 = DNF, -2 = no hay suficientes */
__attribute__((unused)) static int average(int n) {
    if (ntimes < n) return -2;
    int sum = 0, lo = 0x7fffffff, hi = 0, dnf = 0;
    for (int i = 0; i < n; i++) {
        int t = times[i];
        if (t < 0) { dnf++; continue; }
        sum += t;
        if (t < lo) lo = t;
        if (t > hi) hi = t;
    }
    if (dnf > 1) return -1;
    if (dnf == 1) return (sum - lo) / (n - 2);           /* el DNF es el peor */
    return (sum - lo - hi) / (n - 2);
}

static void fmt(wchar_t *b, int ms) {
    if (ms == -1) { lstrcpyW(b, L"DNF"); return; }
    if (ms < 0) { lstrcpyW(b, L"-"); return; }
    int cs = ms / 10;
    if (cs >= 6000) wsprintfW(b, L"%d:%02d.%02d", cs / 6000, cs / 100 % 60, cs % 100);
    else wsprintfW(b, L"%d.%02d", cs / 100, cs % 100);
}

/* giro animándose: FPS_MOVE; tiempo corriendo (centésimas): FPS_CLOCK; inspección
 * (segundos): FPS_COUNTER; reposo: 0 */
static void update_fps(void) {
    widget_set_fps(animating() ? FPS_MOVE : (state == RUN ? FPS_CLOCK : (state == INSPECT ? FPS_COUNTER : 0)));
}

static void tick(float dt) {
    anim_tick(dt);
    update_fps();
}

static void start_solve(double now) {
    penalty = 0;
    if (state == INSPECT) {
        double ins = now - t0;
        penalty = ins > 17 ? -1 : (ins > 15 ? 2000 : 0);
    }
    state = RUN;
    t0 = now;
    moves = 0;
    update_fps();
}

static void finish(double now) {
    int ms = (int)((now - t0) * 1000 + 0.5);
    last_cs = ms;
    last_moves = moves;
    last_pen = penalty;
    last_ms = penalty < 0 ? -1 : ms + penalty;
    record(last_ms);
    state = IDLE;
    update_fps();
}

/* Esc: con el tiempo corriendo cuenta como DNF (convención de csTimer) */
static void abort_solve(void) {
    if (state == RUN) {
        last_ms = -1; last_pen = -1; last_moves = moves; last_cs = (int)((now_seconds() - t0) * 1000);
        record(-1);
    }
    state = IDLE;
    reset_cube();
    sync_vis();
    update_fps();
}

/* el widget dejó de poder usarse: la medición se cancela sin penalizar ni guardar */
static void cancel_solve(void) {
    if (state == IDLE) return;
    state = IDLE;
    reset_cube();
    sync_vis();
    update_fps();
}

static int key(int vk, int scan, int down) {
    if (!down) return 0;
    if (down == 2) return 1;                      /* sin autorepeticion */
    if (vk == VK_ESCAPE) { abort_solve(); widget_redraw(); return 1; }
    if (vk >= VK_LEFT && vk <= VK_DOWN) {        /* camara, como en csTimer */
        if (vk == VK_LEFT) cam_t++;
        else if (vk == VK_RIGHT) cam_t--;
        else if (vk == VK_UP) cam_p++;
        else cam_p--;
        cam_t = cam_t < -6 ? -6 : (cam_t > 6 ? 6 : cam_t);
        cam_p = cam_p < -6 ? -6 : (cam_p > 6 ? 6 : cam_p);
        cfg_set_int(L"cam_t", cam_t);
        cfg_set_int(L"cam_p", cam_p);
        setup_camera();
        widget_redraw();
        return 1;
    }
    if (vk == VK_SPACE) {
        if (state == IDLE) {
            scramble();
            sync_vis();                           /* la mezcla se aplica sin animacion */
            state = use_ins ? INSPECT : READY;
            last_best = 0;
            t0 = now_seconds();
            update_fps();
            widget_redraw();
        }
        return 1;
    }
    if (GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_MENU) < 0) return 0;
    const Key *k = key_for(scan);
    if (!k) return 0;
    widget_post(L"keys", MSG_KEY_USED, scan);   /* resalta la tecla si "keys" esta abierto */
    double now = now_seconds();
    if ((state == INSPECT || state == READY) && !k->rot) start_solve(now);
    do_move(k->ax, k->lo, k->hi, k->q);
    anim_push(k->ax, k->lo, k->hi, k->q);
    if (state == RUN) {
        if (!k->rot) moves++;
        if (is_solved()) finish(now);
    }
    update_fps();
    widget_redraw();
    return 1;
}

/* ------------------------------------------------------------ dibujo */

/* Camara igual que la de csTimer: perspectiva de 30 grados, a 2*sqrt(2) del
 * centro, mirando a (0, -0.075, 0). Por defecto esta de frente y 45 grados
 * por encima (se ven U y F). Las flechas la mueven en pasos de 7.5 grados
 * (hasta +-45), como en csTimer. El cubo mide 1 unidad (pegatina = 1/3). */
#define FOCAL 261.0f            /* escala: en el angulo de camara mas ancho el cubo llega al margen */
static int cam_t = 0, cam_p = 6; /* pasos de 7.5 grados */
static float cpos[3], cr[3], cu[3], cf[3];

static void norm3(float *v) {
    float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    v[0] /= l; v[1] /= l; v[2] /= l;
}

static void setup_camera(void) {
    const float step = 6.2831853f / 48;
    float t = cam_t * step, p = cam_p * step, rad = 2.8284271f;
    cpos[0] = rad * cosf(p) * sinf(t);
    cpos[1] = rad * sinf(p);
    cpos[2] = rad * cosf(p) * cosf(t);
    cf[0] = -cpos[0]; cf[1] = -0.075f - cpos[1]; cf[2] = -cpos[2];
    norm3(cf);
    cr[0] = -cf[2]; cr[1] = 0; cr[2] = cf[0];          /* f x (0,1,0) */
    norm3(cr);
    cu[0] = cr[1] * cf[2] - cr[2] * cf[1];               /* r x f */
    cu[1] = cr[2] * cf[0] - cr[0] * cf[2];
    cu[2] = cr[0] * cf[1] - cr[1] * cf[0];
}

static void project(const float *v, float *out) {
    float d[3] = { v[0] / 3 - cpos[0], v[1] / 3 - cpos[1], v[2] / 3 - cpos[2] };
    float x = d[0] * cr[0] + d[1] * cr[1] + d[2] * cr[2];
    float y = d[0] * cu[0] + d[1] * cu[1] + d[2] * cu[2];
    float z = d[0] * cf[0] + d[1] * cf[1] + d[2] * cf[2];
    out[0] = CX + x / z * FOCAL;
    out[1] = CY - y / z * FOCAL;
}

/* rota v un angulo (coseno co, seno si) alrededor del eje 'axis' (mano derecha) */
static void rotate(float *v, int axis, float co, float si) {
    int a = (axis + 1) % 3, b = (axis + 2) % 3;
    float va = v[a], vb = v[b];
    v[a] = va * co - vb * si;
    v[b] = va * si + vb * co;
}

/* esquinas en pantalla de una placa de media anchura hu x hv sobre la cara ax */
static void quad(int ax, int s, float ca, float cb, float hu, float hv, int rax, float co, float si, float *q) {
    static const float su[4] = { -1, 1, 1, -1 }, sv[4] = { -1, -1, 1, 1 };
    int a = (ax + 1) % 3, b = (ax + 2) % 3;
    float v[3];
    for (int k = 0; k < 4; k++) {
        v[ax] = s * 1.5f;
        v[a] = ca + su[k] * hu;
        v[b] = cb + sv[k] * hv;
        if (rax >= 0) rotate(v, rax, co, si);
        project(v, q + 2 * k);
    }
}

static float dist2d(const float *p, const float *q) {
    float dx = p[0] - q[0], dy = p[1] - q[1];
    return sqrtf(dx * dx + dy * dy);
}

/* El borde negro mide lo mismo en 3D en todas las caras, pero en una cara muy
 * inclinada la pegatina queda de pocos pixeles de ancho y el borde (suavizado,
 * nunca menos de ~1 px) se come el color. Por eso el borde se adelgaza en la
 * direccion en que la pegatina se ve angosta, y desaparece por debajo de
 * BORDER_MIN_PX. El hueco transparente entre pegatinas no cambia. */
#ifndef BORDER_MIN_PX
#define BORDER_MIN_PX 8.0f
#endif
#ifndef BORDER_FULL_PX
#define BORDER_FULL_PX 14.0f
#endif
static float border_for(float len) {
    float k = (len - BORDER_MIN_PX) / (BORDER_FULL_PX - BORDER_MIN_PX);
    return BORDER * (k < 0 ? 0 : k > 1 ? 1 : k);
}

static void sticker(int ax, int s, float ca, float cb, int rax, float co, float si, Color c) {
    float q[8];
    quad(ax, s, ca, cb, STICKER_HALF, STICKER_HALF, rax, co, si, q);
    /* grosor en pantalla en cada direccion = area / largo de los lados paralelos
     * (una cara inclinada se ve como un rombo aplastado: lados largos, grosor chico) */
    float area = 0;
    for (int k = 0; k < 4; k++) {
        int j = (k + 1) & 3;
        area += q[2 * k] * q[2 * j + 1] - q[2 * j] * q[2 * k + 1];
    }
    area = fabsf(area) * 0.5f;
    float eu = (dist2d(q, q + 2) + dist2d(q + 6, q + 4)) * 0.5f;   /* lados en direccion u */
    float ev = (dist2d(q + 2, q + 4) + dist2d(q, q + 6)) * 0.5f;   /* lados en direccion v */
    float bu = border_for(ev > 0 ? area / ev : 0), bv = border_for(eu > 0 ? area / eu : 0);
    if (bu > 0 || bv > 0) {
        gfx_poly(q, 4, BODY_COL);
        quad(ax, s, ca, cb, STICKER_HALF - bu, STICKER_HALF - bv, rax, co, si, q);
    }
    gfx_poly(q, 4, c);
}

/* Como csTimer: no hay cuerpo, cada pegatina es una placa suelta de doble cara
 * (90% de la celda) con un borde negro fino. Por los huecos se ven las
 * pegatinas de las caras de atras. Se dibujan de la mas lejana a la mas cercana. */
static void draw_cube(void) {
    unsigned char order[54];
    float depth[54], co[54], si[54];
    signed char rax[54];
    for (int i = 0; i < 54; i++) {
        const Sticker *t = &vis[i];
        int axis = -1;
        float ang = anim_angle(t, &axis);
        rax[i] = (signed char)(ang != 0 ? axis : -1);
        co[i] = cosf(ang);
        si[i] = sinf(ang);
        float c[3] = { (float)t->p[0], (float)t->p[1], (float)t->p[2] };
        for (int k = 0; k < 3; k++) if (t->n[k]) c[k] = t->n[k] * 1.5f;
        if (rax[i] >= 0) rotate(c, rax[i], co[i], si[i]);
        for (int k = 0; k < 3; k++) c[k] = c[k] / 3.0f - cpos[k];
        depth[i] = c[0] * cf[0] + c[1] * cf[1] + c[2] * cf[2];
        order[i] = (unsigned char)i;
    }
    for (int i = 1; i < 54; i++) {           /* insercion: de lejos a cerca */
        unsigned char o = order[i];
        int j = i - 1;
        while (j >= 0 && depth[order[j]] < depth[o]) { order[j + 1] = order[j]; j--; }
        order[j + 1] = o;
    }
    for (int i = 0; i < 54; i++) {
        int o = order[i];
        const Sticker *t = &vis[o];
        int ax = t->n[0] ? 0 : (t->n[1] ? 1 : 2), s = t->n[ax];
        int a = (ax + 1) % 3, b = (ax + 2) % 3;
        sticker(ax, s, t->p[a], t->p[b], rax[o], co[o], si[o], FACE_COL[t->col]);
    }
}

/* Solo se muestran el tiempo y el cubo (preferencia del usuario). Las medias se
 * siguen guardando en el .ini aunque no se dibujen. */
static void draw(void) {
    wchar_t b[48];
    gfx_clear_hit(); /* sin panel: solo un fondo invisible para poder arrastrarlo */

    double now = now_seconds();
    Color tc = theme->text;
    if (state == RUN) fmt(b, (int)((now - t0) * 1000));
    else if (state == INSPECT) {
        double e = now - t0;
        if (e > 17) { lstrcpyW(b, tr(T_DNF)); tc = theme->error; }
        else if (e > 15) { lstrcpyW(b, tr(T_PLUS2)); tc = theme->error; }
        else { wsprintfW(b, L"%d", 15 - (int)e); tc = theme->accent; }
    } else if (state == READY) lstrcpyW(b, L"0.00");
    else {
        if (last_ms == -1) lstrcpyW(b, tr(T_DNF));
        else fmt(b, last_ms < -1 ? 0 : last_ms);
        if (last_pen == 2000) lstrcatW(b, L"+");
        if (last_ms == -1) tc = theme->error;
        else if (last_best) tc = theme->accent;   /* récord nuevo */
    }
    gfx_text(W / 2, -3, b, tc, 28, ALIGN_CENTER | TEXT_SHADOW);

    draw_cube();

    /* avisos abajo, sobre el cubo: sin teclado (siempre) o cómo empezar (hasta el primer resultado) */
    const wchar_t *note = 0;
    Color nc = theme->accent;
    if (!focused) note = state == IDLE ? tr(T_CLICK_KEYBOARD) : tr_base(TB_NO_KEYBOARD);
    else if (state == IDLE && last_ms == -2) { note = tr(T_HINT_START); nc = theme->sub; }
    if (note) gfx_text_wrap(W / 2, H - MARGIN - 17 - (gfx_text_lines(note, 12, 0) - 1) * 18, note, nc, 12, ALIGN_CENTER | TEXT_SHADOW);
}

/* ------------------------------------------------------------ mapa de teclas
 * El mapa ("keys") es un acompanante: el mismo cube.exe abierto con --keys.
 * Se activa desde el menu, se recuerda, y se cierra cuando se cierra el cubo. */
int keys_main(void);
static int map_on;

static void open_map(void) {
    int x, y, w, h;
    if (!widget_rect(&x, &y, &w, &h)) return;      /* modo foto: no hay ventana */
    if (cfg_get_int_in(L"keys", L"x", -99999) == -99999) {
        /* primera vez: a la izquierda del cubo (o a la derecha si no cabe), alineado abajo */
        RECT r = { x, y, x + w, y + h };
        MONITORINFO mi = { sizeof mi };
        GetMonitorInfoW(MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST), &mi);
        int tw = KEYS_W * w / W, th = KEYS_H * h / H;
        int tx = x - tw - 8;
        if (tx < mi.rcWork.left) tx = x + w + 8;
        cfg_set_int_in(L"keys", L"x", tx);
        cfg_set_int_in(L"keys", L"y", y + h - th);
        cfg_set_int_in(L"keys", L"opacity", cfg_get_int(L"opacity", 100));
    }
    wchar_t exe[MAX_PATH], cmd[MAX_PATH + 16];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    wsprintfW(cmd, L"\"%s\" --keys", exe);
    STARTUPINFOW si = { sizeof si };
    PROCESS_INFORMATION pi;
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

static void close_map(void) { widget_post(L"keys", MSG_CLOSE_MAP, 0); }

static void message(int code, int value) {
    (void)value;
    if (code == MSG_MAP_CLOSED) { map_on = 0; cfg_set_int(L"map", 0); }
}

/* al cerrar el cubo se cierra su mapa, pero la opcion queda guardada para la proxima vez */
static void closed(int by_user) { (void)by_user; close_map(); }

/* ------------------------------------------------------------ menu y arranque */

/* velocidades de giro de csTimer: duración de cada giro en ms (0 = instantáneo) */
static const int SPEEDS[4] = { 0, 50, 100, 200 };
enum {
    M_INSPECTION, M_KEYMAP, M_SPEED, M_SPEED_0, M_SPEED_50, M_SPEED_100, M_SPEED_200, M_SPEED_END,
    M_SEP, M_CLEAR, M_CLEAR_YES, M_CLEAR_END, M_COUNT
};
static const int MENU[M_COUNT + 1] = {
    [M_INSPECTION] = T_INSPECTION, [M_KEYMAP] = T_KEYMAP,
    [M_SPEED] = T_SPEED | MENU_SUB,
    [M_SPEED_0] = T_SPEED_0, [M_SPEED_50] = T_SPEED_50, [M_SPEED_100] = T_SPEED_100, [M_SPEED_200] = T_SPEED_200,
    [M_SPEED_END] = MENU_END,
    [M_SEP] = MENU_SEP,
    [M_CLEAR] = T_CLEAR | MENU_SUB, [M_CLEAR_YES] = T_CLEAR_YES, [M_CLEAR_END] = MENU_END,
};
static const int HELP[] = { T_H_SPACE, T_H_KEYS, T_H_ARROWS, T_H_ESC, 0 };

static int menu_checked(int i) {
    if (i >= M_SPEED_0 && i <= M_SPEED_200) return turn_ms == SPEEDS[i - M_SPEED_0] ? CHECK_RADIO : CHECK_NONE;
    if (i == M_INSPECTION) return use_ins;
    if (i == M_KEYMAP) return map_on;
    return CHECK_NONE;
}

static void on_menu(int i) {
    if (i == M_INSPECTION) { use_ins = !use_ins; cfg_set_int(L"inspection", use_ins); }
    else if (i == M_KEYMAP) {
        map_on = !map_on;
        cfg_set_int(L"map", map_on);
        if (map_on) open_map(); else close_map();
    } else if (i == M_CLEAR_YES) {
        ntimes = 0; best = 0; last_ms = -2; last_pen = 0;
        save_stats();
    } else if (i >= M_SPEED_0 && i <= M_SPEED_200) {
        turn_ms = SPEEDS[i - M_SPEED_0];
        cfg_set_int(L"turn_ms", turn_ms);
    }
}

static void focus(int f) { focused = f; }
static void init(void) {
    /* TEMPORAL (quitar antes de publicar): claves y mapa del nombre provisional */
    cfg_rename(L"inspeccion", L"inspection");
    cfg_rename(L"giro_ms", L"turn_ms");
    cfg_rename(L"mapa", L"map");
    cfg_migrate_section(L"keys", L"cubo", L"teclas");

    reset_cube();
    sync_vis();
    load_stats();
    turn_ms = cfg_get_choice(L"turn_ms", 100, SPEEDS, 4);
    cam_t = cfg_get_range(L"cam_t", 0, -6, 6);
    cam_p = cfg_get_range(L"cam_p", 6, -6, 6);
    setup_camera();
    map_on = cfg_get_range(L"map", 0, 0, 1);
    if (map_on) open_map();
}
static void hide(int h) { if (h) cancel_solve(); }   /* medición: se cancela sin penalizar */

int main(void) {
    if (wcsstr(GetCommandLineW(), L"--keys")) return keys_main();
    WidgetDesc d = {
        .name = L"cube", .w = W, .h = H, .fps = 0,
        .on_init = init, .on_tick = tick, .on_draw = draw, .on_key = key, .on_hide = hide, .on_focus = focus,
        .on_message = message, .on_close = closed,
        .texts = { TXT_ES, TXT_EN }, .ntexts = T_COUNT,
        .menu = MENU, .menu_checked = menu_checked, .on_menu = on_menu, .help = HELP,
        .legacy_ini = L"cubo",
    };
    return widget_run(&d);
}
