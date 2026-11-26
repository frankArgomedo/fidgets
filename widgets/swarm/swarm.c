/*
 * swarm - survivors-like de bolsillo: te mueves con WASD, atacas solo y sobrevives a
 * oleadas que crecen sin fin; al subir de nivel eliges una mejora.
 *
 * PROTOTIPO (paso 2 de ideas/swarm.md): sirve para decidir la celda, las tarjetas y
 * medir el consumo con la pantalla llena. La ficha se escribe al cerrar el diseño;
 * mientras tanto, lo decidido está en ideas/swarm.md. Falta: guardar la partida al
 * cerrar (necesita un bloque de estado en la base), ícono, casos definitivos.
 *
 * Perilla solo del prototipo (variable de entorno; se quita cuando la partida
 * guardada permita cargar estados para las fotos y la medición):
 *   SWARM_FILL = 1     pantalla llena: enemigos al tope, todos los tipos, tres ataques
 *                      en nivel 3 y el jugador no muere
 *   SWARM_LOG = ruta   al morir, agrega una línea a ese archivo con el ataque inicial,
 *                      el tiempo, el nivel y las mejoras (para calibrar la simulación)
 */
#include "widget.h"
#include <windows.h>
#include <math.h>

#define W 240
#define H 240
#define N_CARDS 3   /* tarjetas al subir de nivel */
#define MAXE 64     /* enemigos en memoria: los ~54 visibles más los que vienen llegando */
#define MAXP 32     /* proyectiles */
#define MAXG 32     /* gemas */
#define PI 3.14159265f

/* Colores del juego (representan cosas del juego; nunca para texto ni avisos). */
#define COL_PLAYER RGBA(240, 242, 248, 240)
#define COL_ENEMY  RGBA(235, 85, 85, 235)
#define COL_GEM    RGBA(50, 195, 170, 235)
#define COL_ATTACK RGBA(140, 190, 255, 235)

enum { T_NONE, T_TITLE, T_BEST, T_NO_RECORD, T_NEW_RECORD, T_PICK, T_CLICK_PICK,
       T_SHOT, T_AURA, T_ORBIT, T_WAVE, T_DMG, T_RATE, T_AREA, T_SPEED, T_MAGNET, T_HEALTH,
       T_CLEAR, T_CLEAR_YES, T_H_MOVE, T_H_PICK, T_H_RESUME, T_COUNT };
static const wchar_t *const TXT_ES[T_COUNT] = {
    [T_TITLE] = L"Enjambre",
    [T_BEST] = L"Mejor: %s",
    [T_NO_RECORD] = L"—",
    [T_NEW_RECORD] = L"¡nuevo récord!",
    [T_PICK] = L"elige tu ataque",
    [T_CLICK_PICK] = L"clic: elige tu ataque",
    [T_SHOT] = L"Disparo", [T_AURA] = L"Aura", [T_ORBIT] = L"Órbita", [T_WAVE] = L"Onda",
    [T_DMG] = L"Daño", [T_RATE] = L"Cadencia", [T_AREA] = L"Alcance", [T_SPEED] = L"Velocidad",
    [T_MAGNET] = L"Imán", [T_HEALTH] = L"Vida",
    [T_CLEAR] = L"Borrar récord", [T_CLEAR_YES] = L"Sí, borrarlo",
    [T_H_MOVE] = L"WASD o flechas · moverte",
    [T_H_PICK] = L"Clic o 1–4 · elegir tarjeta",
    [T_H_RESUME] = L"Clic · seguir tras la pausa",
};
static const wchar_t *const TXT_EN[T_COUNT] = {
    [T_TITLE] = L"Swarm",
    [T_BEST] = L"Best: %s",
    [T_NEW_RECORD] = L"new record!",
    [T_PICK] = L"pick your attack",
    [T_CLICK_PICK] = L"click: pick your attack",
    [T_SHOT] = L"Shot", [T_AURA] = L"Aura", [T_ORBIT] = L"Orbit", [T_WAVE] = L"Wave",
    [T_DMG] = L"Damage", [T_RATE] = L"Fire rate", [T_AREA] = L"Area", [T_SPEED] = L"Speed",
    [T_MAGNET] = L"Magnet", [T_HEALTH] = L"Health",
    [T_CLEAR] = L"Clear record", [T_CLEAR_YES] = L"Yes, clear it",
    [T_H_MOVE] = L"WASD or arrows · move",
    [T_H_PICK] = L"Click or 1–4 · pick a card",
    [T_H_RESUME] = L"Click · resume after a pause",
};

/* ------------------------------------------------------------ mejoras */
enum { IT_SHOT, IT_AURA, IT_ORBIT, IT_WAVE, IT_DMG, IT_RATE, IT_AREA, IT_SPEED, IT_MAGNET, IT_HEALTH, IT_COUNT };
#define N_ATTACKS 4
#define MAX_ATTACKS 3
#define MAX_PASSIVES 4
#define MAX_LEVEL 5
static int lvl[IT_COUNT];

/* por nivel (índice 0 = nivel 1) */
static const int   SHOT_N[5]   = { 1, 2, 2, 2, 3 };
static const float SHOT_DMG[5] = { 10, 10, 15, 15, 15 };
static const float SHOT_CD[5]  = { 1.0f, 1.0f, 1.0f, 1.0f, 0.8f };
static const float AURA_R[5]   = { 24, 30, 30, 36, 36 };
static const float AURA_DMG[5] = { 5, 5, 7, 7, 9 };
static const int   ORB_N[5]    = { 1, 2, 2, 3, 3 };
static const float ORB_R[5]    = { 32, 32, 32, 32, 40 };
static const float ORB_T[5]    = { 2, 2, 2, 2, 1.5f };
static const float ORB_DMG[5]  = { 10, 10, 14, 14, 14 };
static const float WAVE_CD[5]  = { 4, 4, 3, 3, 3 };
static const float WAVE_R[5]   = { 60, 72, 72, 72, 84 };
static const float WAVE_DMG[5] = { 12, 12, 12, 18, 18 };
static const float WAVE_PUSH[5] = { 20, 20, 20, 20, 32 };

static float dmg_mul(void)  { return 1 + 0.10f * lvl[IT_DMG]; }
static float rate_mul(void) { return 1 - 0.08f * lvl[IT_RATE]; }
static float area_mul(void) { return 1 + 0.10f * lvl[IT_AREA]; }
static float speed(void)    { return 70 * (1 + 0.08f * lvl[IT_SPEED]); }
static float magnet(void)   { return 16 + 12.0f * lvl[IT_MAGNET]; }
static float hp_max(void)   { return 100 + 20.0f * lvl[IT_HEALTH]; }

/* ----------------------------------------------------------- entidades */
enum { EN_BASIC, EN_FAST, EN_SLOW, EN_BOSS, EN_TYPES };
static const float EN_SIZE[EN_TYPES] = { 12, 10, 14, 24 };
static const float EN_HP[EN_TYPES]   = { 10, 5, 40, 300 };
static const float EN_SPD[EN_TYPES]  = { 40, 75, 25, 35 };
static const float EN_DMG[EN_TYPES]  = { 10, 6, 15, 20 };
static const int   EN_XP[EN_TYPES]   = { 1, 1, 3, 0 };   /* el jefe deja la gema grande */

/* straight = 1: va en línea recta (vx, vy) y desaparece al alejarse (estampida, muro) */
typedef struct { float x, y, hp, orb_cd, flash, vx, vy; int type, alive, wave_id, straight; } Enemy;
typedef struct { float x, y, vx, vy, life, dmg; int pierce, alive, last; } Shot;
typedef struct { float x, y; int xp, big, alive; } Gem;
static Enemy en[MAXE];
static Shot  sh[MAXP];
static Gem   gm[MAXG];

/* ------------------------------------------------------------- estado */
enum { START, PLAY, CARDS, RESULT };
static int fill, start_attack;
static wchar_t log_path[MAX_PATH];
static int state = START, paused, focused, best, new_record;
static float px, py, hp, invuln, game_t, spawn_acc, next_boss, ceil_t = -1;
static float shot_cd, aura_cd, orb_ang, wave_cd, wave_t = -1;
/* eventos al azar dentro de ventanas (ver begin): llegada de tipos, jefe, formaciones, olas */
static float t_fast, t_slow, next_form, surge_end, calm_end, warn_t, warn_a;
static int last_form = -1;
static int wave_id, level, xp, pending, held[4];
static int card_item[4], card_n;

static float view_r(void) { return sqrtf((float)(W * W + H * H)) / 2; }   /* centro -> esquina */
static int cap_visible(void) {   /* 15 % de cobertura con enemigos de 12 px: 54 */
    return (int)(0.15f * (W - 2 * MARGIN) * (H - 2 * MARGIN) / 144.0f);
}
static float frand(void) { return (rnd() & 0xFFFFFF) / (float)0x1000000; }
static float frange(float lo, float hi) { return lo + (hi - lo) * frand(); }
static int xp_need(void) { return 5 + 4 * (level - 1); }

/* curva de dificultad (ideas/swarm.md): multiplicadores según el minuto */
#define HP_GROWTH 1.27f   /* vida de los enemigos: x1,27 por minuto desde el min 2, sin saltos */
static float hp_curve(float m) {
    float h = m < 2 ? 1 : powf(HP_GROWTH, m - 2);
    /* recta final: desde que todo está al máximo, x1,5 por minuto en vez de x1,27 */
    if (ceil_t >= 0 && game_t > ceil_t) h *= powf(1.5f / HP_GROWTH, (game_t - ceil_t) / 60);
    return h;
}
/* velocidad: básico y lento aceleran entre el min 4 y el 12; el rápido ya va al tope */
static const float EN_SPD_MAX[4] = { 55, 75, 35, 35 };
static float en_speed(int type, float m) {
    float t = m < 4 ? 0 : m > 12 ? 1 : (m - 4) / 8;
    return EN_SPD[type] + (EN_SPD_MAX[type] - EN_SPD[type]) * t;
}
/* el lento y el jefe resisten el empuje y el frenado (les afecta la mitad; al jefe, el
 * empuje no le afecta) */
static float resist(int type) { return type == EN_SLOW || type == EN_BOSS ? 0.5f : 1; }
static float dmg_curve(float m) { float d = powf(1.1f, floorf(m / 2)); return d > 2 ? 2 : d; }

static void spawn(int type, int on_screen) {
    for (int i = 0; i < MAXE; i++) if (!en[i].alive) {
        Enemy *e = &en[i];
        float a = frand() * 2 * PI, d;
        if (on_screen) d = 40 + frand() * (W / 2 - 46);          /* solo SWARM_FILL */
        else d = view_r() + EN_SIZE[type];
        e->x = px + cosf(a) * d; e->y = py + sinf(a) * d;
        e->type = type; e->alive = 1; e->flash = 0; e->orb_cd = 0; e->wave_id = -1; e->straight = 0;
        e->hp = EN_HP[type] * hp_curve(game_t / 60);
        return;
    }
}

static int pick_type(void) {   /* el rápido y el lento llegan en un momento al azar (begin) */
    float r = frand();
    if (!fill && game_t < t_fast) return EN_BASIC;
    if (!fill && game_t < t_slow) return r < 0.8f ? EN_BASIC : EN_FAST;
    return r < 0.6f ? EN_BASIC : r < 0.85f ? EN_FAST : EN_SLOW;
}

static void drop_gem(float x, float y, int xpv, int big) {
    int fi = -1; float fd = -1;
    for (int i = 0; i < MAXG; i++) {
        if (!gm[i].alive) { gm[i] = (Gem){ x, y, xpv, big, 1 }; return; }
        float d = (gm[i].x - px) * (gm[i].x - px) + (gm[i].y - py) * (gm[i].y - py);
        if (d > fd) { fd = d; fi = i; }
    }
    /* tope lleno: la más lejana (la que quedó atrás) se suma a la nueva, que cae cerca
     * del jugador; no se pierde experiencia y no queda lejos */
    gm[fi] = (Gem){ x, y, gm[fi].xp + xpv, gm[fi].big | big, 1 };
}

static void hurt(Enemy *e, float dmg) {
    e->hp -= dmg;
    e->flash = widget_reduce_motion() ? 0 : 0.1f;
    if (e->hp <= 0) { e->alive = 0; drop_gem(e->x, e->y, EN_XP[e->type], e->type == EN_BOSS); }
}

/* ------------------------------------------------------------- tarjetas */
static int owned(int first, int last) {
    int n = 0;
    for (int i = first; i < last; i++) n += lvl[i] > 0;
    return n;
}

static void deal_cards(void) {
    int cand[IT_COUNT], nc = 0;
    int atk = owned(0, N_ATTACKS), pas = owned(N_ATTACKS, IT_COUNT);
    for (int i = 0; i < IT_COUNT; i++) {
        if (lvl[i] >= MAX_LEVEL) continue;
        if (!lvl[i] && (i < N_ATTACKS ? atk >= MAX_ATTACKS : pas >= MAX_PASSIVES)) continue;
        cand[nc++] = i;
    }
    if (nc == 0) {   /* todo al máximo: recta final (ya no cura; ver hp_curve) */
        if (ceil_t < 0) ceil_t = game_t;
        pending = 0;
        return;
    }
    for (int i = nc - 1; i > 0; i--) { int j = (int)(rnd() % (unsigned)(i + 1)); int t = cand[i]; cand[i] = cand[j]; cand[j] = t; }
    card_n = nc < N_CARDS ? nc : N_CARDS;
    for (int i = 0; i < card_n; i++) card_item[i] = cand[i];
    state = CARDS;
    widget_set_fps(0);
}

static void card_rect(int i, int *x, int *y, int *w, int *h) {
    int aw = W - 2 * MARGIN, gap = 6;
    if (state == START) {   /* 2x2 debajo del encabezado */
        int top = MARGIN + 20, ah = H - MARGIN - top;
        *w = (aw - gap) / 2; *h = (ah - gap) / 2; if (*h > 76) *h = 76;
        int oy = top + (ah - (2 * *h + gap)) / 2;
        *x = MARGIN + (i % 2) * (*w + gap); *y = oy + (i / 2) * (*h + gap);
    } else {
        *w = (aw - (card_n - 1) * gap) / card_n; *h = 66;
        *x = MARGIN + i * (*w + gap); *y = (H - *h) / 2;
    }
}

static void begin(int attack);
static void choose(int i) {
    if (state == START) { begin(i); return; }
    if (i >= card_n) return;
    lvl[card_item[i]]++;
    if (card_item[i] == IT_HEALTH) hp += 20;
    pending--;
    state = PLAY;
    if (pending > 0) deal_cards();
    if (state == PLAY) widget_set_fps(FPS_MOVE);
    widget_redraw();
}

/* ------------------------------------------------------------- partida */
static void begin(int attack) {
    for (int i = 0; i < IT_COUNT; i++) lvl[i] = 0;
    for (int i = 0; i < MAXE; i++) en[i].alive = 0;
    for (int i = 0; i < MAXP; i++) sh[i].alive = 0;
    for (int i = 0; i < MAXG; i++) gm[i].alive = 0;
    lvl[attack] = 1;
    start_attack = attack;
    px = py = 0; hp = hp_max(); invuln = 0; game_t = 0; spawn_acc = 0; ceil_t = -1;
    /* ventanas (ideas/swarm.md): mismo reparto para todos, distinto en cada partida */
    t_fast = frange(90, 150); t_slow = frange(210, 270);
    next_boss = frange(300, 420); next_form = frange(300, 420); last_form = -1; warn_t = 0;
    surge_end = frange(25, 45); calm_end = surge_end + frange(8, 12);
    shot_cd = aura_cd = wave_cd = 0; orb_ang = 0; wave_t = -1; wave_id = 0;
    level = 1; xp = 0; pending = 0; paused = 0; new_record = 0;
    if (fill) {
        lvl[IT_SHOT] = lvl[IT_AURA] = lvl[IT_ORBIT] = 3;
        for (int i = 0; i < cap_visible(); i++) spawn(pick_type(), 1);
        spawn(EN_BOSS, 0);
        for (int i = 0; i < 12; i++) drop_gem(px + (frand() - 0.5f) * W * 0.8f, py + (frand() - 0.5f) * H * 0.8f, 1, 0);
    }
    state = PLAY;
    widget_set_fps(FPS_MOVE);
    widget_redraw();
}

/* solo prototipo (SWARM_LOG): agrega una línea (con su fin de línea) al registro */
static void log_line(const char *b, int n) {
    if (!log_path[0]) return;
    HANDLE f = CreateFileW(log_path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD wr;
    WriteFile(f, b, (DWORD)n, &wr, NULL);
    CloseHandle(f);
}

/* una línea por partida terminada */
static void log_run(void) {
    static const char *const ID[IT_COUNT] = { "shot", "aura", "orbit", "wave", "dmg", "rate", "area", "speed", "magnet", "health" };
    if (!log_path[0]) return;
    char b[256];
    int c = ceil_t < 0 ? -1 : (int)ceil_t;
    int n = wsprintfA(b, "inicio=%s tiempo=%d:%02d segundos=%d nivel=%d techo=%d mejoras=", ID[start_attack],
                      (int)game_t / 60, (int)game_t % 60, (int)game_t, level, c);
    for (int i = 0; i < IT_COUNT; i++) if (lvl[i]) n += wsprintfA(b + n, "%s%d,", ID[i], lvl[i]);
    b[n - 1] = '\r'; b[n++] = '\n';   /* la última coma pasa a ser el fin de línea */
    log_line(b, n);
}

static void game_over(void) {
    log_run();
    state = RESULT;
    int t = (int)game_t;
    new_record = t > best;
    if (new_record) { best = t; cfg_set_int(L"best", best); }
    widget_set_fps(0);
}

static void fire(void) {
    int L = lvl[IT_SHOT] - 1, n = SHOT_N[L], nt = 0;
    Enemy *t[3];   /* los n más cercanos dentro de la vista, sin repetir */
    float lim = W / 2.0f;
    for (int k = 0; k < n; k++) {
        Enemy *b = NULL; float bd = lim * lim;
        for (int i = 0; i < MAXE; i++) if (en[i].alive) {
            int dup = 0;
            for (int j = 0; j < nt; j++) dup |= t[j] == &en[i];
            float dx = en[i].x - px, dy = en[i].y - py, d = dx * dx + dy * dy;
            if (!dup && d < bd) { bd = d; b = &en[i]; }
        }
        if (b) t[nt++] = b;
    }
    if (!nt) return;
    for (int k = 0; k < n; k++) {
        /* si hay menos enemigos que proyectiles, los que sobran van al más cercano, abiertos */
        Enemy *e = t[k < nt ? k : 0];
        float a = atan2f(e->y - py, e->x - px) + (k < nt ? 0 : (k - nt + 1) * 0.2f);
        for (int i = 0; i < MAXP; i++) if (!sh[i].alive) {
            sh[i] = (Shot){ px, py, cosf(a) * 180, sinf(a) * 180, (W / 2.0f + 20) / 180, SHOT_DMG[L] * dmg_mul(), L >= 3, 1, -1 };
            break;
        }
    }
    shot_cd = SHOT_CD[L] * rate_mul();
}

/* ------------------------------------------------------------ formaciones
 * Desde un momento al azar entre el min 5 y el 7, cada 35–60 s, una formación al azar
 * entre las ya desbloqueadas, nunca la misma dos veces seguidas. Aparecen fuera de la
 * vista; la estampida y el muro se anuncian con una flecha en el borde (warn_*). */
enum { F_RING, F_STAMPEDE, F_WALL, F_DOUBLE, F_COUNT };
static const float F_UNLOCK[F_COUNT] = { 0, 0, 7 * 60, 9 * 60 };   /* desde qué segundo */

/* deja al menos n lugares libres: si no hay, se liberan los enemigos más lejanos */
static void make_room(int n) {
    int libres = 0;
    for (int i = 0; i < MAXE; i++) libres += !en[i].alive;
    while (libres < n) {
        int fi = -1; float fd = -1;
        for (int i = 0; i < MAXE; i++) if (en[i].alive && en[i].type != EN_BOSS) {
            float d = (en[i].x - px) * (en[i].x - px) + (en[i].y - py) * (en[i].y - py);
            if (d > fd) { fd = d; fi = i; }
        }
        if (fi < 0) break;
        en[fi].alive = 0; libres++;
    }
}

static Enemy *place(int type, float x, float y, float vx, float vy) {
    for (int i = 0; i < MAXE; i++) if (!en[i].alive) {
        Enemy *e = &en[i];
        e->x = x; e->y = y; e->vx = vx; e->vy = vy; e->straight = vx != 0 || vy != 0;
        e->type = type; e->alive = 1; e->flash = 0; e->orb_cd = 0; e->wave_id = -1;
        e->hp = EN_HP[type] * hp_curve(game_t / 60);
        return e;
    }
    return NULL;
}

/* anillo de 18 posiciones con un hueco de 3 (~60°), radio r, hueco hacia gap_a */
static void ring_at(float r, float gap_a, float m) {
    for (int k = 0; k < 15; k++) {
        float a = gap_a + (k + 2) * 2 * PI / 18;
        place(m >= 10 && k % 3 == 1 ? EN_SLOW : EN_BASIC, px + cosf(a) * r, py + sinf(a) * r, 0, 0);
    }
}

/* fila de n enemigos que entra desde el ángulo a y cruza en línea recta por donde
 * está el jugador */
static void line_at(int type, int n, float spacing, float a, float spd, float lead) {
    /* arranca a lead segundos de entrar en la vista: lo que dura el aviso */
    float ca = cosf(a), sa = sinf(a), r = W / 2.0f + spd * lead;
    for (int k = 0; k < n; k++) {
        float o = (k - (n - 1) / 2.0f) * spacing;
        place(type, px + ca * r - sa * o, py + sa * r + ca * o, -ca * spd, -sa * spd);
    }
}

static void formation(float m) {
    int cand[F_COUNT], nc = 0;
    for (int f = 0; f < F_COUNT; f++) if (game_t >= F_UNLOCK[f] && f != last_form) cand[nc++] = f;
    int f = cand[rnd() % (unsigned)nc];
    last_form = f;
    {   /* registro (prototipo): qué formación y cuándo */
        static const char *const FN[F_COUNT] = { "anillo", "estampida", "muro", "anillo_doble" };
        char b[64];
        log_line(b, wsprintfA(b, "  formacion=%s segundo=%d\r\n", FN[f], (int)game_t));
    }
    float a = frand() * 2 * PI;
    make_room(f == F_DOUBLE ? 30 : 15);
    switch (f) {
    case F_RING:     ring_at(view_r() + 10, a, m); break;
    case F_DOUBLE:   ring_at(view_r() + 10, a, m); ring_at(view_r() + 40, a + PI, m); break;
    case F_STAMPEDE: line_at(EN_FAST, 8, 14, a, 75, 2); warn_a = a; warn_t = 2; break;
    case F_WALL:     line_at(EN_SLOW, 7, 16, a, en_speed(EN_SLOW, m), 2); warn_a = a; warn_t = 2; break;
    }
}

static void tick(float dt) {
    if (state != PLAY || paused) { widget_set_fps(0); return; }
    game_t += dt;
    float m = game_t / 60;

    /* moverse */
    float mx = (float)(held[3] - held[2]), my = (float)(held[1] - held[0]);
    if (mx || my) { float n = sqrtf(mx * mx + my * my); px += mx / n * speed() * dt; py += my / n * speed() * dt; }
    if (invuln > 0) invuln -= dt;
    if (lvl[IT_HEALTH]) { hp += 0.3f * lvl[IT_HEALTH] * dt; if (hp > hp_max()) hp = hp_max(); }

    /* aparecer: ritmo que sube hasta el min 4; olas de largo al azar (25–45 s fuertes,
     * 8–12 s de respiro) */
    int alive = 0;
    for (int i = 0; i < MAXE; i++) alive += en[i].alive && en[i].type != EN_BOSS && !en[i].straight;
    if (game_t >= calm_end) { surge_end = game_t + frange(25, 45); calm_end = surge_end + frange(8, 12); }
    float rate = 0.7f + 2.8f * (m < 4 ? m / 4 : 1);
    rate *= game_t < surge_end ? 1.3f : 0.4f;
    spawn_acc += rate * dt;
    while (spawn_acc >= 1) { spawn_acc -= 1; if (fill || alive < cap_visible()) { spawn(pick_type(), 0); alive++; } }
    if (game_t >= next_boss) { spawn(EN_BOSS, 0); next_boss += frange(150, 210); }
    if (game_t >= next_form) { formation(m); next_form += frange(35, 60); }
    if (warn_t > 0) warn_t -= dt;

    /* enemigos: van hacia el jugador, se empujan entre sí, reaparecen por delante */
    float aura_r = lvl[IT_AURA] ? AURA_R[lvl[IT_AURA] - 1] * area_mul() : 0;
    float far_d = view_r() + 40, edmg = dmg_curve(m);
    for (int i = 0; i < MAXE; i++) if (en[i].alive) {
        Enemy *e = &en[i];
        if (e->flash > 0) e->flash -= dt;
        if (e->orb_cd > 0) e->orb_cd -= dt;
        float dx = px - e->x, dy = py - e->y, d = sqrtf(dx * dx + dy * dy) + 0.001f;
        if (e->straight) {
            e->x += e->vx * dt; e->y += e->vy * dt;
            if (d > far_d && e->vx * dx + e->vy * dy < 0) { e->alive = 0; continue; }   /* ya pasó */
            if (d < EN_SIZE[e->type] / 2 + 6 && invuln <= 0 && !fill) {
                hp -= EN_DMG[e->type] * edmg;
                invuln = 0.5f;
                if (hp <= 0) { game_over(); return; }
            }
            continue;
        }
        if (d > far_d && e->type != EN_BOSS) {
            float a = (mx || my) ? atan2f(my, mx) + (frand() - 0.5f) * 2.0f : frand() * 2 * PI;
            e->x = px + cosf(a) * (view_r() + EN_SIZE[e->type]); e->y = py + sinf(a) * (view_r() + EN_SIZE[e->type]);
            continue;
        }
        float v = en_speed(e->type, m);
        if (lvl[IT_AURA] == 5 && d < aura_r) v *= 1 - 0.3f * resist(e->type);
        e->x += dx / d * v * dt; e->y += dy / d * v * dt;
        float rr = EN_SIZE[e->type] / 2 + 6;
        if (d < rr && invuln <= 0 && !fill) {
            hp -= EN_DMG[e->type] * edmg;
            invuln = 0.5f;
            if (hp <= 0) { game_over(); return; }
        }
    }
    for (int i = 0; i < MAXE; i++) if (en[i].alive) for (int j = i + 1; j < MAXE; j++) if (en[j].alive) {
        float dx = en[j].x - en[i].x, dy = en[j].y - en[i].y, md = (EN_SIZE[en[i].type] + EN_SIZE[en[j].type]) / 2;
        float d2 = dx * dx + dy * dy;
        if (d2 >= md * md || d2 < 0.0001f) continue;
        float d = sqrtf(d2), push = (md - d) * 0.5f;
        dx /= d; dy /= d;
        en[i].x -= dx * push; en[i].y -= dy * push; en[j].x += dx * push; en[j].y += dy * push;
    }

    /* ataques */
    if (lvl[IT_SHOT]) { shot_cd -= dt; if (shot_cd <= 0) fire(); }
    for (int i = 0; i < MAXP; i++) if (sh[i].alive) {
        Shot *s = &sh[i];
        s->x += s->vx * dt; s->y += s->vy * dt; s->life -= dt;
        if (s->life <= 0) { s->alive = 0; continue; }
        float sr = 2 * area_mul();
        for (int k = 0; k < MAXE; k++) if (en[k].alive && k != s->last) {
            float dx = en[k].x - s->x, dy = en[k].y - s->y, r = EN_SIZE[en[k].type] / 2 + sr;
            if (dx * dx + dy * dy > r * r) continue;
            hurt(&en[k], s->dmg);
            s->last = k;
            if (s->pierce-- <= 0) { s->alive = 0; break; }
        }
    }
    if (lvl[IT_AURA]) {
        aura_cd -= dt;
        if (aura_cd <= 0) {
            aura_cd = 0.5f * rate_mul();
            for (int k = 0; k < MAXE; k++) if (en[k].alive) {
                float dx = en[k].x - px, dy = en[k].y - py, r = aura_r + EN_SIZE[en[k].type] / 2;
                if (dx * dx + dy * dy < r * r) hurt(&en[k], AURA_DMG[lvl[IT_AURA] - 1] * dmg_mul());
            }
        }
    }
    if (lvl[IT_ORBIT]) {
        int L = lvl[IT_ORBIT] - 1;
        orb_ang += dt * 2 * PI / (ORB_T[L] * rate_mul());
        float r = ORB_R[L] * area_mul(), br = 4 * area_mul();
        for (int b = 0; b < ORB_N[L]; b++) {
            float a = orb_ang + b * 2 * PI / ORB_N[L], bx = px + cosf(a) * r, by = py + sinf(a) * r;
            for (int k = 0; k < MAXE; k++) if (en[k].alive && en[k].orb_cd <= 0) {
                float dx = en[k].x - bx, dy = en[k].y - by, rr = EN_SIZE[en[k].type] / 2 + br;
                if (dx * dx + dy * dy < rr * rr) { hurt(&en[k], ORB_DMG[L] * dmg_mul()); en[k].orb_cd = 0.5f; }
            }
        }
    }
    if (lvl[IT_WAVE]) {
        int L = lvl[IT_WAVE] - 1;
        wave_cd -= dt;
        if (wave_cd <= 0) { wave_cd = WAVE_CD[L] * rate_mul(); wave_t = 0; wave_id++; }
        if (wave_t >= 0) {
            wave_t += dt;
            float r = WAVE_R[L] * area_mul() * (wave_t > 0.3f ? 1 : wave_t / 0.3f);
            for (int k = 0; k < MAXE; k++) if (en[k].alive && en[k].wave_id != wave_id) {
                float dx = en[k].x - px, dy = en[k].y - py, d = sqrtf(dx * dx + dy * dy) + 0.001f;
                if (d > r + EN_SIZE[en[k].type] / 2) continue;
                en[k].wave_id = wave_id;
                if (en[k].type != EN_BOSS) {
                    float pu = WAVE_PUSH[L] * resist(en[k].type);
                    en[k].x += dx / d * pu; en[k].y += dy / d * pu;
                }
                hurt(&en[k], WAVE_DMG[L] * dmg_mul());
            }
            if (wave_t > 0.3f) wave_t = -1;
        }
    }

    /* gemas: el imán las atrae; al tocarlas suman experiencia */
    for (int i = 0; i < MAXG; i++) if (gm[i].alive) {
        Gem *g = &gm[i];
        float dx = px - g->x, dy = py - g->y, d = sqrtf(dx * dx + dy * dy) + 0.001f;
        if (d < 8) {
            g->alive = 0;
            xp += g->big ? (xp_need() - xp > g->xp ? xp_need() - xp : g->xp) : g->xp;
            while (xp >= xp_need()) { xp -= xp_need(); level++; pending++; }
        } else if (d < magnet()) { g->x += dx / d * 220 * dt; g->y += dy / d * 220 * dt; }
    }
    if (pending > 0 && !fill) deal_cards();
}

/* ------------------------------------------------------------- dibujo */
/* en pausa o eligiendo tarjeta, el juego se atenúa (sin panel de fondo) */
static int faded;
static Color gc(Color c) { return faded ? color_alpha(c, (int)(c >> 24) / 4) : c; }

static float sx(float x) { return x - px + W / 2.0f; }
static float sy(float y) { return y - py + H / 2.0f; }

static void ring(float cx, float cy, float r, float dot, Color c) {
    int n = (int)(r * 0.6f) + 8;
    for (int i = 0; i < n; i++) { float a = i * 2 * PI / n; gfx_circle(cx + cosf(a) * r, cy + sinf(a) * r, dot, c); }
}

static void tri(float cx, float cy, float ang, float len, float half_w, Color c) {
    float ca = cosf(ang), sa = sinf(ang), p[6] = {
        cx + ca * len * 0.6f, cy + sa * len * 0.6f,
        cx - ca * len * 0.4f - sa * half_w, cy - sa * len * 0.4f + ca * half_w,
        cx - ca * len * 0.4f + sa * half_w, cy - sa * len * 0.4f - ca * half_w };
    gfx_poly(p, 3, c);
}

static void square(float cx, float cy, float h, Color c) {
    float p[8] = { cx - h, cy - h, cx + h, cy - h, cx + h, cy + h, cx - h, cy + h };
    gfx_poly(p, 4, c);
}

static void diamond(float cx, float cy, float h, Color c) {
    float p[8] = { cx, cy - h, cx + h, cy, cx, cy + h, cx - h, cy };
    gfx_poly(p, 4, c);
}

static void draw_enemy(const Enemy *e) {
    float x = sx(e->x), y = sy(e->y), s = EN_SIZE[e->type];
    Color c = gc(e->flash > 0 ? color_mix(COL_ENEMY, COL_PLAYER, 0.6f) : COL_ENEMY);
    float a = atan2f(py - e->y, px - e->x);
    switch (e->type) {
    case EN_BASIC: tri(x, y, a, s, s * 0.5f, c); break;
    case EN_FAST:  tri(x, y, a, s * 1.2f, s * 0.3f, c); break;
    case EN_SLOW:  square(x, y, s / 2, c); break;
    case EN_BOSS:  square(x, y, s / 2, c); square(x, y, s / 2 - 4, gc(color_mix(COL_ENEMY, theme->ink, 0.55f))); break;
    }
}

static void boss_arrow(void) {
    for (int i = 0; i < MAXE; i++) if (en[i].alive && en[i].type == EN_BOSS) {
        float x = sx(en[i].x), y = sy(en[i].y), h = EN_SIZE[EN_BOSS] / 2;
        if (x > -h && x < W + h && y > -h && y < H + h) continue;   /* se ve: sin flecha */
        float a = atan2f(y - H / 2.0f, x - W / 2.0f), ca = cosf(a), sa = sinf(a);
        float lim = MARGIN + 6, kx = (W / 2.0f - lim) / (fabsf(ca) + 1e-4f), ky = (H / 2.0f - lim) / (fabsf(sa) + 1e-4f);
        float k = kx < ky ? kx : ky;
        tri(W / 2.0f + ca * k, H / 2.0f + sa * k, a, 11, 5, gc(theme->accent));   /* aviso: no es un enemigo */
    }
}

/* aviso de formación: flecha en el borde, del lado por donde viene */
static void form_arrow(void) {
    if (warn_t <= 0) return;
    float ca = cosf(warn_a), sa = sinf(warn_a), lim = MARGIN + 6;
    float kx = (W / 2.0f - lim) / (fabsf(ca) + 1e-4f), ky = (H / 2.0f - lim) / (fabsf(sa) + 1e-4f);
    float k = kx < ky ? kx : ky;
    for (int i = -1; i <= 1; i++) {   /* tres flechas en fila: distinta de la del jefe */
        float ox = -sa * i * 14, oy = ca * i * 14;
        tri(W / 2.0f + ca * k + ox, H / 2.0f + sa * k + oy, warn_a, 9, 4, gc(theme->accent));
    }
}

static void icon(int item, float cx, float cy) {
    Color c = COL_ATTACK;
    switch (item) {
    case IT_SHOT:   gfx_circle(cx + 5, cy, 3, c); gfx_rect((int)cx - 9, (int)cy - 1, 10, 2, color_alpha(c, 150)); break;
    case IT_AURA:   gfx_circle(cx, cy, 9, color_alpha(c, 60)); ring(cx, cy, 9, 1.1f, c); gfx_circle(cx, cy, 3, COL_PLAYER); break;
    case IT_ORBIT:  gfx_circle(cx, cy, 3, COL_PLAYER); for (int i = 0; i < 3; i++) gfx_circle(cx + cosf(i * 2.1f) * 9, cy + sinf(i * 2.1f) * 9, 2.5f, c); break;
    case IT_WAVE:   ring(cx, cy, 10, 1.0f, c); ring(cx, cy, 5, 1.0f, color_alpha(c, 150)); break;
    case IT_DMG:    tri(cx, cy + 1, -PI / 2, 18, 7, c); break;
    case IT_RATE:   for (int i = 0; i < 3; i++) gfx_rect((int)cx - 8 + i * 6, (int)cy - 3 - i * 3, 4, 6 + i * 3 + 3, c); break;
    case IT_AREA:   for (int i = 0; i < 4; i++) tri(cx + cosf(i * PI / 2) * 6, cy + sinf(i * PI / 2) * 6, i * PI / 2, 8, 4, c); break;
    case IT_SPEED:  tri(cx - 4, cy, 0, 12, 6, c); tri(cx + 5, cy, 0, 12, 6, c); break;
    case IT_MAGNET: ring(cx, cy, 9, 1.0f, color_alpha(c, 150)); diamond(cx, cy, 5, COL_GEM); break;
    case IT_HEALTH: gfx_rect((int)cx - 2, (int)cy - 8, 5, 17, c); gfx_rect((int)cx - 8, (int)cy - 2, 17, 5, c); break;
    }
}

static void draw_card(int i, int item, int next_lvl) {
    int x, y, w, h;
    card_rect(i, &x, &y, &w, &h);
    gfx_round_rect(x, y, w, h, 6, theme->surface);
    float iy = (float)y + h / 2.0f - 14;
    icon(item, x + w / 2.0f, iy);
    gfx_text(x + w / 2, (int)iy + 14, tr(T_SHOT + item), theme->text, 11, ALIGN_CENTER | TEXT_SHADOW);
    for (int k = 0; k < MAX_LEVEL; k++)   /* nivel: 5 puntitos (no depende del idioma) */
        gfx_circle(x + w / 2.0f + (k - 2) * 7, (float)y + h - 8, 2, k < next_lvl ? theme->accent : theme->line);
    if (focused) {   /* número de la tecla que la elige */
        wchar_t n[4]; wsprintfW(n, L"%d", i + 1);
        gfx_text(x + 5, y + 3, n, theme->sub, 11, ALIGN_LEFT | TEXT_SHADOW);
    }
}

static void fmt_time(wchar_t *b, int s) { wsprintfW(b, L"%d:%02d", s / 60, s % 60); }

static void play_button(float cx, float cy) {
    gfx_circle(cx, cy, 20, theme->ok);
    for (int i = 0; i < 12; i++) gfx_rect((int)cx - 5 + i, (int)cy - (12 - i) / 2, 1, 12 - i, theme->ink);
}

static void draw_best_line(int y) {
    wchar_t buf[48], num[16];
    if (state == RESULT && new_record) { gfx_text(W / 2, y, tr(T_NEW_RECORD), theme->accent, 12, ALIGN_CENTER | TEXT_SHADOW); return; }
    if (best > 0) fmt_time(num, best); else lstrcpyW(num, tr(T_NO_RECORD));
    wsprintfW(buf, tr(T_BEST), num);
    gfx_text(W / 2, y, buf, theme->sub, 12, ALIGN_CENTER | TEXT_SHADOW);
}

static void draw(void) {
    wchar_t buf[32];
    gfx_clear_hit();

    if (state == START) {
        gfx_text(W / 2, MARGIN + 1, tr(focused ? T_PICK : T_CLICK_PICK), theme->sub, 12, ALIGN_CENTER | TEXT_SHADOW);
        for (int i = 0; i < N_ATTACKS; i++) draw_card(i, i, 1);
        return;
    }
    if (state == RESULT) {
        draw_best_line(MARGIN + 1);
        fmt_time(buf, (int)game_t);
        gfx_text(W / 2, H / 2 - 40, buf, new_record ? theme->accent : theme->text, 40, ALIGN_CENTER | TEXT_SHADOW);
        play_button(W / 2.0f, H / 2.0f + 36);
        return;
    }

    faded = state == CARDS || paused;
    /* rejilla tenue: decorativa, para que se note el movimiento */
    int g = 24, ox = (int)floorf(px) % g, oy = (int)floorf(py) % g;
    if (ox < 0) ox += g;
    if (oy < 0) oy += g;
    for (int y = MARGIN + g - oy; y < H - MARGIN; y += g)
        for (int x = MARGIN + g - ox; x < W - MARGIN; x += g) gfx_rect(x, y, 1, 1, theme->line);

    for (int i = 0; i < MAXG; i++) if (gm[i].alive) diamond(sx(gm[i].x), sy(gm[i].y), gm[i].big ? 5 : gm[i].xp > 3 ? 4 : 3, gc(COL_GEM));   /* la que acumula, más grande */
    for (int i = 0; i < MAXE; i++) if (en[i].alive) draw_enemy(&en[i]);
    float cx = W / 2.0f, cy = H / 2.0f;
    if (lvl[IT_AURA]) {
        float r = AURA_R[lvl[IT_AURA] - 1] * area_mul();
        gfx_circle(cx, cy, r, gc(color_alpha(COL_ATTACK, 28)));
        ring(cx, cy, r, 0.8f, gc(color_alpha(COL_ATTACK, 120)));
    }
    if (lvl[IT_WAVE] && wave_t >= 0) {
        int L = lvl[IT_WAVE] - 1;
        ring(cx, cy, WAVE_R[L] * area_mul() * (wave_t / 0.3f > 1 ? 1 : wave_t / 0.3f), 1.2f, gc(COL_ATTACK));
    }
    if (lvl[IT_ORBIT]) {
        int L = lvl[IT_ORBIT] - 1;
        float r = ORB_R[L] * area_mul();
        for (int b = 0; b < ORB_N[L]; b++) {
            float a = orb_ang + b * 2 * PI / ORB_N[L];
            gfx_circle(cx + cosf(a) * r, cy + sinf(a) * r, 4 * area_mul(), gc(COL_ATTACK));
        }
    }
    for (int i = 0; i < MAXP; i++) if (sh[i].alive) gfx_circle(sx(sh[i].x), sy(sh[i].y), 2 * area_mul(), gc(COL_ATTACK));
    int blink = invuln > 0 && !widget_reduce_motion() && ((int)(invuln * 12.5f) & 1);
    gfx_circle(cx, cy, 6, gc(blink ? color_alpha(COL_PLAYER, 110) : COL_PLAYER));
    boss_arrow();
    form_arrow();

    /* marcador: tiempo, vida y experiencia */
    fmt_time(buf, (int)game_t);
    gfx_text(MARGIN + 1, MARGIN - 1, buf, theme->text, 14, ALIGN_LEFT | TEXT_SHADOW);
    int bw = 54, bx = W - MARGIN - bw, by = MARGIN + 5;
    gfx_round_rect(bx - 1, by - 1, bw + 2, 8, 4, theme->surface);   /* borde oscuro: se ve en fondo claro */
    int fw = (int)(bw * (hp > 0 ? hp : 0) / hp_max());
    if (fw > 0) gfx_round_rect(bx, by, fw, 6, 3, theme->text);
    gfx_round_rect(MARGIN, H - MARGIN - 3, W - 2 * MARGIN, 3, 1, theme->surface);
    int xw = (W - 2 * MARGIN) * xp / xp_need();
    if (xw > 0) gfx_round_rect(MARGIN, H - MARGIN - 3, xw, 3, 1, COL_GEM);

    if (state == CARDS) {
        for (int i = 0; i < card_n; i++) draw_card(i, card_item[i], lvl[card_item[i]] + 1);
    } else if (paused) {
        gfx_text_wrap(W / 2, H / 2 - 8, tr_base(TB_PAUSED), theme->accent, 12, ALIGN_CENTER | TEXT_SHADOW);
    }
}

/* ------------------------------------------------------------- entradas */
static int mouse(int x, int y, int ev) {
    if (ev != MOUSE_DOWN) return 0;
    if (state == PLAY && paused) { paused = 0; widget_set_fps(FPS_MOVE); widget_redraw(); return 1; }
    if (state == START || state == CARDS) {
        int n = state == START ? N_ATTACKS : card_n;
        for (int i = 0; i < n; i++) {
            int cx, cy, cw, ch;
            card_rect(i, &cx, &cy, &cw, &ch);
            if (x >= cx && x < cx + cw && y >= cy && y < cy + ch) { choose(i); return 1; }
        }
        return 0;
    }
    if (state == RESULT) {
        float dx = x - W / 2.0f, dy = y - (H / 2.0f + 36);
        if (dx * dx + dy * dy > 400) return 0;
        state = START; widget_redraw();
        return 1;
    }
    return 0;
}

static int key(int vk, int scan, int down) {
    (void)vk;
    int dir = -1;
    switch (scan) {
    case 0x11: case 0x148: dir = 0; break;   /* W, arriba */
    case 0x1F: case 0x150: dir = 1; break;   /* S, abajo */
    case 0x1E: case 0x14B: dir = 2; break;   /* A, izquierda */
    case 0x20: case 0x14D: dir = 3; break;   /* D, derecha */
    }
    if (dir >= 0) { held[dir] = down != 0; return 1; }
    if (down == 1 && scan >= 0x02 && scan <= 0x05 && (state == START || state == CARDS)) { choose(scan - 0x02); return 1; }
    return 0;
}

static void pause_game(void) {
    if (state == PLAY && !paused) { paused = 1; widget_set_fps(0); }
    for (int i = 0; i < 4; i++) held[i] = 0;
}

static void hide(int hidden) { if (hidden) pause_game(); widget_redraw(); }
static void focus(int f) { focused = f; if (!f) pause_game(); widget_redraw(); }

static void init(void) {
    wchar_t v[8];
    if (GetEnvironmentVariableW(L"SWARM_FILL", v, 8) > 0) fill = 1;
    if (GetEnvironmentVariableW(L"SWARM_LOG", log_path, MAX_PATH) >= MAX_PATH) log_path[0] = 0;
    best = cfg_get_range(L"best", 0, 0, 1000000);
}

enum { M_CLEAR, M_CLEAR_YES, M_CLEAR_END, M_COUNT };
static const int MENU[M_COUNT + 1] = { [M_CLEAR] = T_CLEAR | MENU_SUB, [M_CLEAR_YES] = T_CLEAR_YES, [M_CLEAR_END] = MENU_END };
static const int HELP[] = { T_H_MOVE, T_H_PICK, T_H_RESUME, 0 };

static void on_menu(int i) {
    if (i == M_CLEAR_YES) { best = 0; new_record = 0; cfg_set_int(L"best", 0); widget_redraw(); }
}

int main(void) {
    WidgetDesc d = {
        .name = L"swarm", .w = W, .h = H, .fps = 0,
        .on_init = init, .on_tick = tick, .on_draw = draw,
        .on_key = key, .on_mouse = mouse, .on_hide = hide, .on_focus = focus,
        .texts = { TXT_ES, TXT_EN }, .ntexts = T_COUNT,
        .menu = MENU, .on_menu = on_menu, .help = HELP,
    };
    return widget_run(&d);
}
