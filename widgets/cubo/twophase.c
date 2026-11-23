/*
 * twophase.c - algoritmo de dos fases de Kociemba (implementacion propia).
 *
 * Fase 1: lleva el cubo al subgrupo <U, D, R2, L2, F2, B2> (orientaciones
 *         correctas y aristas del medio en su capa). Coordenadas: twist
 *         (orientacion de esquinas, 2187), flip (orientacion de aristas, 2048)
 *         y slice (posicion de las 4 aristas del medio, 495).
 * Fase 2: resuelve dentro del subgrupo. Coordenadas: permutacion de esquinas
 *         (40320), de las 8 aristas U/D (40320) y de las 4 del medio (24).
 *
 * Memoria: tablas de movimientos ~1.8 MB + tablas de poda (4 bits) ~2 MB.
 */
#include <stdlib.h>
#include <string.h>
#include "twophase.h"

enum { URF, UFL, ULB, UBR, DFR, DLF, DBL, DRB };
enum { UR, UF, UL, UB, DR, DF, DL, DB, FR, FL, BL, BR };

typedef struct { uint8_t cp[8], co[8], ep[12], eo[12]; } Cubie;

/* Los 6 giros basicos (horarios) en notacion "la posicion i recibe la pieza X". */
static const Cubie BASIC[6] = {
    /* U */ { { UBR, URF, UFL, ULB, DFR, DLF, DBL, DRB }, { 0 },
              { UB, UR, UF, UL, DR, DF, DL, DB, FR, FL, BL, BR }, { 0 } },
    /* R */ { { DFR, UFL, ULB, URF, DRB, DLF, DBL, UBR }, { 2, 0, 0, 1, 1, 0, 0, 2 },
              { FR, UF, UL, UB, BR, DF, DL, DB, DR, FL, BL, UR }, { 0 } },
    /* F */ { { UFL, DLF, ULB, UBR, URF, DFR, DBL, DRB }, { 1, 2, 0, 0, 2, 1, 0, 0 },
              { UR, FL, UL, UB, DR, FR, DL, DB, UF, DF, BL, BR }, { 0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0 } },
    /* D */ { { URF, UFL, ULB, UBR, DLF, DBL, DRB, DFR }, { 0 },
              { UR, UF, UL, UB, DF, DL, DB, DR, FR, FL, BL, BR }, { 0 } },
    /* L */ { { URF, ULB, DBL, UBR, DFR, UFL, DLF, DRB }, { 0, 1, 2, 0, 0, 2, 1, 0 },
              { UR, UF, BL, UB, DR, DF, FL, DB, FR, UL, DL, BR }, { 0 } },
    /* B */ { { URF, UFL, UBR, DRB, DFR, DLF, ULB, DBL }, { 0, 0, 1, 2, 0, 0, 2, 1 },
              { UR, UF, UL, BR, DR, DF, DL, BL, FR, FL, UB, DB }, { 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1 } },
};
static Cubie MOVES[18];
/* movimientos permitidos en la fase 2: U* D* y dobles de R L F B */
static const uint8_t P2[10] = { 0, 1, 2, 4, 7, 9, 10, 11, 13, 16 };

static void mul(const Cubie *a, const Cubie *b, Cubie *out) {
    Cubie r;
    for (int i = 0; i < 8; i++) {
        r.cp[i] = a->cp[b->cp[i]];
        r.co[i] = (uint8_t)((a->co[b->cp[i]] + b->co[i]) % 3);
    }
    for (int i = 0; i < 12; i++) {
        r.ep[i] = a->ep[b->ep[i]];
        r.eo[i] = (uint8_t)((a->eo[b->ep[i]] + b->eo[i]) & 1);
    }
    *out = r;
}

static void identity(Cubie *c) {
    memset(c, 0, sizeof *c);
    for (int i = 0; i < 8; i++) c->cp[i] = (uint8_t)i;
    for (int i = 0; i < 12; i++) c->ep[i] = (uint8_t)i;
}

/* ------------------------------------------------------------ coordenadas */

static int cnk(int n, int k) {
    if (k > n || k < 0) return 0;
    int r = 1;
    for (int i = 0; i < k; i++) r = r * (n - i) / (i + 1);
    return r;
}

static int get_twist(const Cubie *c) { int t = 0; for (int i = 0; i < 7; i++) t = t * 3 + c->co[i]; return t; }
static void set_twist(Cubie *c, int t) {
    int s = 0;
    for (int i = 6; i >= 0; i--) { c->co[i] = (uint8_t)(t % 3); s += t % 3; t /= 3; }
    c->co[7] = (uint8_t)((3 - s % 3) % 3);
}
static int get_flip(const Cubie *c) { int t = 0; for (int i = 0; i < 11; i++) t = t * 2 + c->eo[i]; return t; }
static void set_flip(Cubie *c, int t) {
    int s = 0;
    for (int i = 10; i >= 0; i--) { c->eo[i] = (uint8_t)(t & 1); s += t & 1; t >>= 1; }
    c->eo[11] = (uint8_t)(s & 1);
}
static int get_slice(const Cubie *c) {
    int a = 0, x = 0;
    for (int j = 11; j >= 0; j--)
        if (c->ep[j] >= FR) a += cnk(11 - j, ++x);
    return a;
}
static void set_slice(Cubie *c, int a) {
    int x = 4, o = 0;
    memset(c->ep, 255, 12);
    for (int j = 0; j < 12; j++)
        if (x > 0 && a - cnk(11 - j, x) >= 0) { c->ep[j] = (uint8_t)(FR + 4 - x); a -= cnk(11 - j, x); x--; }
    for (int j = 0; j < 12; j++)
        if (c->ep[j] == 255) c->ep[j] = (uint8_t)o++;
}
/* indice de Lehmer de una permutacion de n elementos (valores 0..n-1) */
static int perm_index(const uint8_t *p, int n) {
    int idx = 0;
    for (int i = 0; i < n; i++) {
        int k = 0;
        for (int j = i + 1; j < n; j++) if (p[j] < p[i]) k++;
        idx = idx * (n - i) + k;
    }
    return idx;
}
static void perm_set(uint8_t *p, int n, int idx) {
    int code[12];
    uint8_t used[12] = { 0 };
    for (int i = n - 1; i >= 0; i--) { code[i] = idx % (n - i); idx /= (n - i); }
    for (int i = 0; i < n; i++) {
        int k = code[i];
        for (int v = 0; v < n; v++)
            if (!used[v] && k-- == 0) { p[i] = (uint8_t)v; used[v] = 1; break; }
    }
}
static int get_cp(const Cubie *c) { return perm_index(c->cp, 8); }
static int get_ud(const Cubie *c) { return perm_index(c->ep, 8); }
static int get_sp(const Cubie *c) {
    uint8_t s[4];
    for (int i = 0; i < 4; i++) s[i] = (uint8_t)(c->ep[8 + i] - 8);
    return perm_index(s, 4);
}

/* ------------------------------------------------------------ tablas */

#define N_TWIST 2187
#define N_FLIP 2048
#define N_SLICE 495
#define N_PERM8 40320

static uint16_t (*twist_mv)[18], (*flip_mv)[18], (*slice_mv)[18];
static uint16_t (*cp_mv)[10], (*ud_mv)[10];
static uint8_t sp_mv[24][10];
static uint8_t *pr_ts, *pr_fs, *pr_cs, *pr_us; /* poda: 2 entradas por byte */
static int ready;

static inline int pget(const uint8_t *t, int i) { return (t[i >> 1] >> ((i & 1) << 2)) & 15; }
static inline void pset(uint8_t *t, int i, int v) {
    t[i >> 1] = (uint8_t)((t[i >> 1] & ~(15 << ((i & 1) << 2))) | (v << ((i & 1) << 2)));
}

/* BFS por capas: a = coord1 (n1 valores) x b = coord2 (n2 valores) */
static uint8_t *build_prune(int n1, int n2, int nm, const uint16_t *mv1, int s1,
                            const void *mv2, int s2, int mv2_bytes) {
    int n = n1 * n2;
    uint8_t *t = malloc((size_t)(n + 1) / 2);
    if (!t) return 0;
    memset(t, 0xFF, (size_t)(n + 1) / 2);
    pset(t, 0, 0);
    int done = 1;
    for (int d = 0; done < n && d < 14; d++) {
        for (int i = 0; i < n; i++) {
            if (pget(t, i) != d) continue;
            int a = i / n2, b = i % n2;
            for (int m = 0; m < nm; m++) {
                int na = mv1[a * s1 + m];
                int nb = mv2_bytes == 1 ? ((const uint8_t *)mv2)[b * s2 + m] : ((const uint16_t *)mv2)[b * s2 + m];
                int j = na * n2 + nb;
                if (pget(t, j) == 15) { pset(t, j, d + 1); done++; }
            }
        }
    }
    return t;
}

int tp_init(void) {
    if (ready) return 1;
    for (int f = 0; f < 6; f++) {
        Cubie c;
        identity(&c);
        for (int p = 0; p < 3; p++) { mul(&c, &BASIC[f], &c); MOVES[f * 3 + p] = c; }
    }
    twist_mv = malloc(sizeof *twist_mv * N_TWIST);
    flip_mv = malloc(sizeof *flip_mv * N_FLIP);
    slice_mv = malloc(sizeof *slice_mv * N_SLICE);
    cp_mv = malloc(sizeof *cp_mv * N_PERM8);
    ud_mv = malloc(sizeof *ud_mv * N_PERM8);
    if (!twist_mv || !flip_mv || !slice_mv || !cp_mv || !ud_mv) return 0;

    Cubie c, r;
    for (int i = 0; i < N_TWIST; i++) {
        identity(&c); set_twist(&c, i);
        for (int m = 0; m < 18; m++) { mul(&c, &MOVES[m], &r); twist_mv[i][m] = (uint16_t)get_twist(&r); }
    }
    for (int i = 0; i < N_FLIP; i++) {
        identity(&c); set_flip(&c, i);
        for (int m = 0; m < 18; m++) { mul(&c, &MOVES[m], &r); flip_mv[i][m] = (uint16_t)get_flip(&r); }
    }
    for (int i = 0; i < N_SLICE; i++) {
        identity(&c); set_slice(&c, i);
        for (int m = 0; m < 18; m++) { mul(&c, &MOVES[m], &r); slice_mv[i][m] = (uint16_t)get_slice(&r); }
    }
    for (int i = 0; i < N_PERM8; i++) {
        identity(&c); perm_set(c.cp, 8, i);
        for (int m = 0; m < 10; m++) { mul(&c, &MOVES[P2[m]], &r); cp_mv[i][m] = (uint16_t)get_cp(&r); }
        identity(&c); perm_set(c.ep, 8, i);
        for (int m = 0; m < 10; m++) { mul(&c, &MOVES[P2[m]], &r); ud_mv[i][m] = (uint16_t)get_ud(&r); }
    }
    for (int i = 0; i < 24; i++) {
        uint8_t s[4];
        identity(&c); perm_set(s, 4, i);
        for (int k = 0; k < 4; k++) c.ep[8 + k] = (uint8_t)(8 + s[k]);
        for (int m = 0; m < 10; m++) { mul(&c, &MOVES[P2[m]], &r); sp_mv[i][m] = (uint8_t)get_sp(&r); }
    }
    pr_ts = build_prune(N_TWIST, N_SLICE, 18, &twist_mv[0][0], 18, &slice_mv[0][0], 18, 2);
    pr_fs = build_prune(N_FLIP, N_SLICE, 18, &flip_mv[0][0], 18, &slice_mv[0][0], 18, 2);
    pr_cs = build_prune(N_PERM8, 24, 10, &cp_mv[0][0], 10, &sp_mv[0][0], 10, 1);
    pr_us = build_prune(N_PERM8, 24, 10, &ud_mv[0][0], 10, &sp_mv[0][0], 10, 1);
    if (!pr_ts || !pr_fs || !pr_cs || !pr_us) return 0;
    ready = 1;
    return 1;
}

/* ------------------------------------------------------------ busqueda */

static Cubie start;
static uint8_t sol[40];
static int max_total;
static long nodes, node_limit;

/* no repetir cara; entre caras opuestas, solo en un orden (U antes que D...) */
static int bad_seq(int last, int m) {
    if (last < 0) return 0;
    int f = m / 3, lf = last / 3;
    return f == lf || f == lf - 3;
}

static int phase2(int cp, int ud, int sp, int depth, int togo, int last) {
    if (togo == 0) return cp == 0 && ud == 0 && sp == 0;
    for (int k = 0; k < 10; k++) {
        int m = P2[k];
        if (bad_seq(last, m)) continue;
        int ncp = cp_mv[cp][k], nud = ud_mv[ud][k], nsp = sp_mv[sp][k];
        int h1 = pget(pr_cs, ncp * 24 + nsp), h2 = pget(pr_us, nud * 24 + nsp);
        if ((h1 > h2 ? h1 : h2) > togo - 1) continue;
        sol[depth] = (uint8_t)m;
        if (phase2(ncp, nud, nsp, depth + 1, togo - 1, m)) return 1;
    }
    return 0;
}

static int is_p2(int m) { for (int k = 0; k < 10; k++) if (P2[k] == m) return 1; return 0; }

static int found_len;

static int start_phase2(int depth1) {
    Cubie c = start;
    for (int i = 0; i < depth1; i++) mul(&c, &MOVES[sol[i]], &c);
    int cp = get_cp(&c), ud = get_ud(&c), sp = get_sp(&c);
    int h1 = pget(pr_cs, cp * 24 + sp), h2 = pget(pr_us, ud * 24 + sp);
    int h = h1 > h2 ? h1 : h2, lim = max_total - depth1;
    if (lim > 12) lim = 12;
    int last = depth1 ? sol[depth1 - 1] : -1;
    for (int d = h; d <= lim; d++)
        if (phase2(cp, ud, sp, depth1, d, last)) { found_len = depth1 + d; return 1; }
    return 0;
}

static int phase1(int tw, int fl, int sl, int depth, int togo, int last) {
    if (++nodes > node_limit) return -1;
    if (togo == 0) {
        if (tw || fl || sl) return 0;
        if (depth > 0 && is_p2(last)) return 0; /* ya se habria visto con menos giros */
        return start_phase2(depth);
    }
    for (int m = 0; m < 18; m++) {
        if (bad_seq(last, m)) continue;
        int nt = twist_mv[tw][m], nf = flip_mv[fl][m], ns = slice_mv[sl][m];
        int h1 = pget(pr_ts, nt * N_SLICE + ns), h2 = pget(pr_fs, nf * N_SLICE + ns);
        if ((h1 > h2 ? h1 : h2) > togo - 1) continue;
        sol[depth] = (uint8_t)m;
        int r = phase1(nt, nf, ns, depth + 1, togo - 1, m);
        if (r) return r;
    }
    return 0;
}

/* resuelve 'start'; devuelve largo de la solucion en sol[] o 0 */
static int solve(void) {
    int tw = get_twist(&start), fl = get_flip(&start), sl = get_slice(&start);
    for (max_total = 21; max_total <= 30; max_total++) {
        nodes = 0;
        node_limit = 2000000;
        int h1 = pget(pr_ts, tw * N_SLICE + sl), h2 = pget(pr_fs, fl * N_SLICE + sl);
        for (int d = h1 > h2 ? h1 : h2; d <= 12 && d <= max_total; d++) {
            int r = phase1(tw, fl, sl, 0, d, -1);
            if (r == 1) return found_len;
            if (r < 0) break;
        }
    }
    return 0;
}

static int parity(const uint8_t *p, int n) {
    int s = 0;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) if (p[j] < p[i]) s ^= 1;
    return s;
}

int tp_random_scramble(uint32_t (*rng)(void), unsigned char *moves, int max) {
    if (!tp_init()) return 0;
    identity(&start);
    for (int i = 7; i > 0; i--) { int j = (int)(rng() % (uint32_t)(i + 1)); uint8_t t = start.cp[i]; start.cp[i] = start.cp[j]; start.cp[j] = t; }
    for (int i = 11; i > 0; i--) { int j = (int)(rng() % (uint32_t)(i + 1)); uint8_t t = start.ep[i]; start.ep[i] = start.ep[j]; start.ep[j] = t; }
    if (parity(start.cp, 8) != parity(start.ep, 12)) { uint8_t t = start.ep[10]; start.ep[10] = start.ep[11]; start.ep[11] = t; }
    set_twist(&start, (int)(rng() % N_TWIST));
    set_flip(&start, (int)(rng() % N_FLIP));

    int n = solve();
    if (n <= 0 || n > max) return 0;
    /* la mezcla es la solucion invertida: orden inverso y giro contrario */
    for (int i = 0; i < n; i++) {
        int m = sol[n - 1 - i];
        moves[i] = (unsigned char)(m / 3 * 3 + (2 - m % 3));
    }
    return n;
}
