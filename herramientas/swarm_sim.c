/*
 * swarm_sim - herramienta de desarrollo: juega muchas partidas de swarm sin ventana,
 * con un bot, y resume cuánto duran. Sirve para ajustar la curva de dificultad
 * (ideas/swarm.md). No forma parte del widget: la compila y corre simular.ps1.
 *
 * Incluye swarm.c tal cual (la misma lógica que el widget) y reemplaza su main.
 * Dos bots (ver bot()); los dos eligen las tarjetas al azar.
 *
 * Uso: swarm_sim.exe [partidas por ataque inicial] [semilla] [ms de reacción del bot]
 */
#define main swarm_main
#include "../widgets/swarm/swarm.c"
#undef main

#include <stdio.h>
#include <stdlib.h>

#define DT (1.0f / FPS_MOVE)
#define MAX_T 1800.0f   /* corta a los 30 min */
static int REACT = 8;   /* pasos entre decisiones del bot: 8 / 45 = ~175 ms, como una persona (calibrado) */

/* Bots: deciden qué teclas mantiene pulsadas en este paso.
 *   BOT_PLAYER: como una persona: esquiva lo cercano, va por las gemas y, si su ataque
 *               es de cerca, se acerca a los enemigos hasta tenerlos a tiro.
 *   BOT_RUNNER: solo huye (nunca se acerca a nada): mide si escapar sin pelear alcanza. */
enum { BOT_PLAYER, BOT_RUNNER };
static int bot_mode;
static float wander;

static Enemy *nearest(float maxd) {
    Enemy *b = NULL; float bd = maxd * maxd;
    for (int i = 0; i < MAXE; i++) if (en[i].alive) {
        float dx = en[i].x - px, dy = en[i].y - py, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; b = &en[i]; }
    }
    return b;
}

static float reach(void) {   /* hasta dónde llega su ataque de cerca (0 = solo disparo) */
    if (lvl[IT_SHOT]) return 0;
    float r = 0;
    if (lvl[IT_AURA]) r = AURA_R[lvl[IT_AURA] - 1] * area_mul();
    if (lvl[IT_ORBIT]) { float o = ORB_R[lvl[IT_ORBIT] - 1] * area_mul(); if (o > r) r = o; }
    if (lvl[IT_WAVE]) { float w = WAVE_R[lvl[IT_WAVE] - 1] * area_mul(); if (w > r) r = w; }
    return r;
}

static void bot(void) {
    float vx = 0, vy = 0;
    /* el jugador con ataque de cerca deja que lo rodeen un poco: solo le asusta lo que
     * está por tocarlo; el que solo huye y el del disparo se alejan desde 70 px */
    float scare = (bot_mode == BOT_PLAYER && reach() > 0) ? 22 : 70;
    for (int i = 0; i < MAXE; i++) if (en[i].alive) {
        float dx = px - en[i].x, dy = py - en[i].y, d = sqrtf(dx * dx + dy * dy) + 0.01f;
        float gap = d - EN_SIZE[en[i].type] / 2 - 6;
        if (gap > scare) continue;
        if (gap < 3) gap = 3;
        float w = (en[i].type == EN_BOSS ? 3.0f : 1.0f) / (gap * gap);
        vx += dx / d * w * 300; vy += dy / d * w * 300;
    }
    if (bot_mode == BOT_PLAYER) {
        float bd = 1e9f, gx = 0, gy = 0;
        for (int i = 0; i < MAXG; i++) if (gm[i].alive) {
            float dx = gm[i].x - px, dy = gm[i].y - py, d = dx * dx + dy * dy;
            if (d < bd) { bd = d; gx = dx; gy = dy; }
        }
        float r = reach();
        if (bd < 120 * 120) { float d = sqrtf(bd) + 0.01f; vx += gx / d; vy += gy / d; }
        else if (r > 0) {   /* ataque de cerca: acercarse al enemigo más cercano */
            Enemy *e = nearest(200);
            if (e) {
                float dx = e->x - px, dy = e->y - py, d = sqrtf(dx * dx + dy * dy) + 0.01f;
                if (d > r * 0.8f) { vx += dx / d; vy += dy / d; }
            }
        }
    }
    if (vx * vx + vy * vy < 0.04f) { wander += 0.01f; vx += cosf(wander) * 0.5f; vy += sinf(wander) * 0.5f; }
    float n = sqrtf(vx * vx + vy * vy);
    for (int k = 0; k < 4; k++) held[k] = 0;
    if (n < 1e-6f) return;
    vx /= n; vy /= n;
    held[3] = vx > 0.38f; held[2] = vx < -0.38f;   /* 8 direcciones */
    held[1] = vy > 0.38f; held[0] = vy < -0.38f;
}

typedef struct { float end, t2, t5, t15, tcap; int level; } Run;

static Run play(int attack) {
    Run r = { 0, -1, -1, -1, -1, 0 };
    best = 1 << 30;   /* nunca hay récord nuevo: la simulación no escribe el .ini */
    wander = frand() * 6.28f;
    begin(attack);
    int think = 0;
    while (state != RESULT && game_t < MAX_T) {
        if (state == CARDS) { choose((int)(rnd() % (unsigned)card_n)); continue; }
        if (--think <= 0) { bot(); think = REACT; }   /* decide cada ~0,2 s, como una persona */
        tick(DT);
        if (level >= 2 && r.t2 < 0) r.t2 = game_t;
        if (level >= 5 && r.t5 < 0) r.t5 = game_t;
        if (level >= 15 && r.t15 < 0) r.t15 = game_t;
        if (level >= 35 && r.tcap < 0) r.tcap = game_t;
    }
    r.end = game_t;
    r.level = level;
    return r;
}

static int cmpf(const void *a, const void *b) { float x = *(const float *)a, y = *(const float *)b; return (x > y) - (x < y); }

/* percentil de los valores >= 0 (los que llegaron); -1 si ninguno */
static float pct(float *v, int n, float p, int *got) {
    float t[4096]; int m = 0;
    for (int i = 0; i < n; i++) if (v[i] >= 0) t[m++] = v[i];
    *got = m;
    if (!m) return -1;
    qsort(t, (size_t)m, sizeof *t, cmpf);
    return t[(int)(p * (m - 1) + 0.5f)];
}

static void mmss(char *b, float s) {
    if (s < 0) { sprintf(b, "  -  "); return; }
    sprintf(b, "%2d:%02d", (int)s / 60, (int)s % 60);
}

int main(int argc, char **argv) {
    int n = argc > 1 ? atoi(argv[1]) : 100;
    if (n < 1) n = 1;
    if (n > 1000) n = 1000;
    if (argc > 2) for (int i = 0, s = atoi(argv[2]); i < s; i++) rnd();
    if (argc > 3 && atoi(argv[3]) > 0) { REACT = (int)(atoi(argv[3]) * FPS_MOVE / 1000.0f + 0.5f); if (REACT < 1) REACT = 1; }
    printf("reaccion del bot: %d ms\n", (int)(REACT * 1000.0f / FPS_MOVE + 0.5f));
    static const char *NAMES[N_ATTACKS] = { "disparo", "aura", "orbita", "onda" };
    static float all_end[4 * 1000];
    for (bot_mode = 0; bot_mode < 2; bot_mode++) {
        int na = 0, g;
        printf("\n== bot %s ==\n", bot_mode == BOT_PLAYER ? "jugador (pelea y recoge gemas)" : "que solo huye");
        printf("%-8s %6s %6s %6s %6s %6s | %6s %6s %6s %6s %6s\n", "inicio", "p10", "p25", "mediana", "p75", "p90",
               "niv2", "niv5", "niv15", "techo", "niv.fin");
        for (int a = 0; a < N_ATTACKS; a++) {
            static float end[1000], t2[1000], t5[1000], t15[1000], tc[1000], lv[1000], after[1000];
            for (int i = 0; i < n; i++) {
                Run r = play(a);
                end[i] = r.end; t2[i] = r.t2; t5[i] = r.t5; t15[i] = r.t15; tc[i] = r.tcap; lv[i] = (float)r.level;
                after[i] = r.tcap >= 0 && r.end < MAX_T ? r.end - r.tcap : -1;   /* cuánto dura tras el techo */
                all_end[na++] = r.end;
            }
            char b[5][8], m2[8], m5[8], m15[8], mc[8], ma[8];
            int g2, g5, g15, gc;
            static const float P[5] = { 0.10f, 0.25f, 0.50f, 0.75f, 0.90f };
            for (int k = 0; k < 5; k++) mmss(b[k], pct(end, n, P[k], &g));
            mmss(m2, pct(t2, n, 0.5f, &g2)); mmss(m5, pct(t5, n, 0.5f, &g5));
            mmss(m15, pct(t15, n, 0.5f, &g15)); mmss(mc, pct(tc, n, 0.5f, &gc));
            int ga; mmss(ma, pct(after, n, 0.5f, &ga));
            printf("%-8s %6s %6s %6s %6s %6s | %6s %6s %6s %6s %6.0f   (llegan: niv15 %d%%, techo %d%%; tras el techo %s)\n",
                   NAMES[a], b[0], b[1], b[2], b[3], b[4], m2, m5, m15, mc, pct(lv, n, 0.5f, &g),
                   g15 * 100 / n, gc * 100 / n, ma);
        }
        char m[8];
        mmss(m, pct(all_end, na, 0.5f, &g));
        printf("mediana total: %s   (%d partidas; corte a los %d min)\n", m, na, (int)MAX_T / 60);
    }
    return 0;
}
