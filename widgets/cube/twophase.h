/*
 * twophase - mezclas "random-state" para 3x3 (como las de la WCA / csTimer).
 *
 * Se elige un estado del cubo al azar (todos igual de probables), se resuelve
 * con el algoritmo de dos fases de Kociemba y la mezcla es la solucion al reves.
 * Las tablas (~4 MB) se generan la primera vez que se pide una mezcla.
 *
 * Movimientos codificados como cara*3 + (giros-1), caras U R F D L B:
 *   0=U 1=U2 2=U'  3=R 4=R2 5=R'  6=F ...  15=B 16=B2 17=B'
 */
#ifndef TWOPHASE_H
#define TWOPHASE_H
#include <stdint.h>

int tp_init(void);  /* 1 si las tablas estan listas */
/* escribe la mezcla en moves (max >= 30); devuelve la cantidad o 0 si falla */
int tp_random_scramble(uint32_t (*rng)(void), unsigned char *moves, int max);

#endif
