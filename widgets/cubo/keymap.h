/*
 * keymap.h - mapa de teclas del cubo virtual (el de csTimer), compartido por
 * los widgets "cubo" y "teclas". Las teclas van por posicion fisica (scan code
 * set 1), asi que funcionan igual con cualquier distribucion de teclado.
 */
#ifndef BORED_KEYMAP_H
#define BORED_KEYMAP_H

/* mensajes entre "cubo" y su mapa "teclas" (widget_post / on_message) */
#define MSG_TECLA_USADA 1   /* cubo -> teclas: value = scan code de la tecla usada */
#define MSG_CERRAR_MAPA 2   /* cubo -> teclas: cerrarse */
#define MSG_MAPA_CERRADO 3  /* teclas -> cubo: el usuario cerro el mapa a mano */

/* tamano logico del mapa (lo usa el cubo para ponerlo a su lado) */
#define TECLAS_W 240   /* cuadricula de 40 px: 6x3 modulos */
#define TECLAS_H 120

/* Mapa de teclas de csTimer, por codigo fisico (scan code set 1). */
typedef struct { unsigned char scan; char label[3]; signed char ax, lo, hi, q, rot; } Key;
static const Key KEYS[] = {
    { 0x17, "R", 0, 1, 1, 3, 0 },    { 0x25, "R'", 0, 1, 1, 1, 0 },   /* I K */
    { 0x11, "B", 2, -1, -1, 1, 0 },  { 0x18, "B'", 2, -1, -1, 3, 0 }, /* W O */
    { 0x1F, "D", 1, -1, -1, 1, 0 },  { 0x26, "D'", 1, -1, -1, 3, 0 }, /* S L */
    { 0x20, "L", 0, -1, -1, 1, 0 },  { 0x12, "L'", 0, -1, -1, 3, 0 }, /* D E */
    { 0x24, "U", 1, 1, 1, 3, 0 },    { 0x21, "U'", 1, 1, 1, 1, 0 },   /* J F */
    { 0x23, "F", 2, 1, 1, 3, 0 },    { 0x22, "F'", 2, 1, 1, 1, 0 },   /* H G */
    { 0x27, "y", 1, -1, 1, 3, 1 },   { 0x1E, "y'", 1, -1, 1, 1, 1 },  /* ; A */
    { 0x16, "r", 0, 0, 1, 3, 0 },    { 0x13, "l'", 0, -1, 0, 3, 0 },  /* U R */
    { 0x32, "r'", 0, 0, 1, 1, 0 },   { 0x2F, "l", 0, -1, 0, 1, 0 },   /* M V */
    { 0x14, "x", 0, -1, 1, 3, 1 },   { 0x15, "x", 0, -1, 1, 3, 1 },   /* T Y */
    { 0x31, "x'", 0, -1, 1, 1, 1 },  { 0x30, "x'", 0, -1, 1, 1, 1 },  /* N B */
    { 0x34, "M'", 0, 0, 0, 3, 0 },   { 0x2D, "M'", 0, 0, 0, 3, 0 },   /* . X */
    { 0x06, "M", 0, 0, 0, 1, 0 },    { 0x07, "M", 0, 0, 0, 1, 0 },    /* 5 6 */
    { 0x02, "S'", 2, 0, 0, 1, 0 },   { 0x0B, "S", 2, 0, 0, 3, 0 },    /* 1 0 */
    { 0x03, "E", 1, 0, 0, 1, 0 },    { 0x0A, "E'", 1, 0, 0, 3, 0 },   /* 2 9 */
    { 0x19, "z", 2, -1, 1, 3, 1 },   { 0x10, "z'", 2, -1, 1, 1, 1 },  /* P Q */
    { 0x2C, "d", 1, -1, 0, 1, 0 },   { 0x35, "d'", 1, -1, 0, 3, 0 },  /* Z / */
    { 0x33, "u", 1, 0, 1, 3, 0 },    { 0x2E, "u'", 1, 0, 1, 1, 0 },   /* , C */
};
#define NKEYS ((int)(sizeof KEYS / sizeof KEYS[0]))

static const Key *key_for(int scan) {
    for (int i = 0; i < NKEYS; i++)
        if (KEYS[i].scan == scan) return &KEYS[i];
    return 0;
}

#endif
