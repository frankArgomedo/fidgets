# swarm: survivors-like de bolsillo

Nombre provisional (`swarm` u `horde`; ninguno choca con un comando de Windows).
Estado: **en diseño**, sin código.

Género: *survivors-like* (también *bullet heaven*). Se toman sus mecánicas (ataque
automático, oleadas que crecen, elegir una mejora al subir de nivel); nunca el nombre,
las armas, los personajes ni el aspecto de un juego concreto.

## Decidido

### Partida infinita
- Sin reloj ni final: la dificultad sube sin parar y la partida termina al morir.
- Récord: el tiempo sobrevivido, contando solo el tiempo de juego (`now_seconds()`,
  sumando solo mientras corre, sin las pausas).
- Al cerrar se guarda la partida y al reabrir queda en «en pausa · clic para seguir»
  (regla «Cerrar»: una partida dura más de un minuto).

### Sin progreso permanente
Cada partida empieza de cero. Dentro de la partida subes de nivel; al terminar, todo se
reinicia. Así los récords son comparables («Justo»).

### Controles
- WASD por scan code (posición física, cualquier distribución); flechas como
  alternativa. Moverse = mantener la tecla (`on_key` recibe pulsar y soltar).
- Juego que avanza solo y se juega con el teclado: se pausa al perder el foco
  («en pausa · clic para seguir»).
- Esc no hace nada: no hay nada que cancelar, y terminar la partida con una tecla que
  se pulsa sin querer iría contra «Justo».

### Cámara y mapa
Cámara sobre el personaje y mapa infinito (lo que define al género: siempre se puede
escapar corriendo).
- Solo existen los enemigos cercanos; los que quedan lejos desaparecen y vuelven a
  aparecer por delante. Coordenadas relativas al jugador: no cuesta memoria.
- Rejilla de puntos tenue para que se note el movimiento (decorativa, alfa baja; no es
  un panel de fondo).
- Descartadas: cámara fija con el mapa del tamaño de la celda (te acorralan en las
  esquinas y no se siente como el género) y mapa cerrado de ~3×3 celdas (agrega paredes
  sin aportar mucho).

### Animaciones
- **Mecánica** (jugador, enemigos, proyectiles): siempre, a `FPS_MOVE`; reducir
  animaciones no la cambia.
- **Efectos que informan** (destello al recibir daño, enemigo que muere): transitorios
  ≤ 300 ms; con reducir animaciones saltan a su final.
- **Adorno** (números de daño, sacudir la pantalla, partículas): no se usan.
- En reposo y en pausa, fps = 0.

### Accesibilidad
Las formas son la segunda señal además del color: jugador = círculo, enemigos =
triángulos, gemas = rombos; el jefe se distingue por el tamaño.

### Curva de dificultad
La partida termina cuando la amenaza supera el poder del jugador. **El poder tiene
techo y la amenaza no**: cada mejora tiene nivel máximo (p.ej. 4 ataques y 6 pasivas
de 5 niveles = 50 subidas); con todo al máximo, subir de nivel solo cura un poco. Así
la partida es infinita en las reglas pero siempre termina, y lo que dura lo deciden
cómo juegas y qué eliges.

Qué sube:

| Qué | Tope | Por qué |
|---|---|---|
| Enemigos por segundo | Los ~54 visibles (15 % de cobertura) | Más no deja ver por dónde escapar |
| Velocidad de los enemigos | ~75 px/s | Límite para reaccionar a tiempo |
| Vida de los enemigos | **Sin tope** | Es lo que hace que la partida termine |
| Daño de los enemigos | Sube poco | Que un solo error no te mate |
| Tipos de enemigo | En minutos fijos | Cambian cómo juegas, no solo cuánto |

Fases (números de partida, se ajustan con la simulación):

| Minuto | Qué pasa | Para qué |
|---|---|---|
| 0–1 | Solo el básico, pocos y lentos; primera subida de nivel a los ~10–15 s | Quien juega 2 min ya eligió mejoras y sintió que crece: las recompensas van al principio |
| 1–4 | Más enemigos hasta el tope; tipo **rápido y débil** (min 2) y **lento con mucha vida** (min 4) | Se arma la build; cada tipo pide otra respuesta |
| 4–8 | Tope de cantidad; vida +25 % por minuto; **jefe** cada 3 min | La build se pone a prueba; el jefe marca hitos |
| 8+ | La vida se duplica cada 2 min; las mejoras ya llegaron al techo | Aquí termina: mediana ~6–8 min, una buena partida pasa los 12 |

- **Olas con respiro:** ciclos de ~40 s (~30 s fuertes, ~10 s tranquilos) dentro de cada
  fase, para recoger gemas y respirar.
- **Jefe:** un enemigo grande (~24 px) con mucha vida, sin ataques especiales.
- **Sin enemigos que disparan** en la primera versión; pueden llegar después como un
  tipo nuevo.

Justo:
- La curva depende solo del **tiempo de juego** (el mismo del récord, que no corre en
  pausa) y es igual para todos: nada de dificultad que se adapta a cómo vas.
- Ningún enemigo aparece a menos de ~100 px del jugador: aparecen fuera de la vista.
- Tras un golpe, ~0,5 s de invulnerabilidad (con su destello transitorio): no se muere
  de un solo golpe.
- Un **modo difícil** (la misma curva, más rápida) con su propio récord queda para
  después.

Cómo se ajusta: una herramienta de simulación (herramienta de desarrollo, como
`llenado.ps1`) que, con `FIDGETS_SEED` y un bot simple (da vueltas esquivando, elige
mejoras al azar), corre cientos de partidas sin ventana y mide cuánto duran. Objetivo:
mediana del bot ~4–5 min, para que una persona que piensa sus mejoras llegue a 6–10.

## Por decidir

### Celda y tamaños
Propuesta: 240×240 (con 160 todo queda diminuto al 25 % de opacidad); jugador ~12 px,
enemigos 10–14 px. Confirmar con fotos.

### Cantidad de enemigos
Enemigos visibles como máximo ≈ cobertura × área útil ÷ área de un enemigo.
Área útil en 240×240: 228×228 ≈ 52.000 px².

| Enemigo | Área | 10 % | 15 % | 20 % |
|---|---|---|---|---|
| 10 px | 100 px² | ~52 | ~78 | ~104 |
| 12 px | 144 px² | ~36 | ~54 | ~72 |
| 14 px | 196 px² | ~26 | ~40 | ~53 |

- Empezar en 15 % de cobertura; con más de ~20 % ya no se ve por dónde escapar.
- Al llegar al tope, la presión sube con más vida, más velocidad o enemigos de otro
  tipo, no con más cantidad. Arreglos fijos: memoria fija y ≤ 3 ms por frame.
- Tiempo para reaccionar: desde el borde hay ~114 px hasta el jugador. Para ≥ 1,5 s,
  los enemigos no pasan de ~75 px/s.

### Tarjetas de mejora: 2 o 3
El juego se pausa y aparecen tarjetas con ícono (~24 px) y nombre (11 px); se eligen
con clic o con 1, 2, 3. En 240 px caben 3 de ~70 px (~10–11 caracteres de nombre). Se
decide con fotos (modo foto y `llenado.ps1` en cada idioma): si 3 no caben, quedan 2.

### Ataques
Cada uno resuelve un problema distinto, para que elegir tenga sentido:

| Ataque | Qué resuelve | Costo |
|---|---|---|
| Disparo al enemigo más cercano | Daño a distancia, a un objetivo | Proyectiles (arreglo fijo) |
| Aura alrededor del jugador | Los que se te pegan | Muy barato: sin entidades |
| Órbita (bolas que giran a tu alrededor) | Defiende la periferia | Pocas entidades |
| Onda cada ciertos segundos | Despeja cuando estás rodeado | Barato: un círculo que crece |
| Rastro al moverte (opcional) | Castiga a los perseguidores; va con WASD | Pocas zonas |

Mejoras pasivas: daño, cadencia, alcance, velocidad, imán de gemas, vida o
regeneración. Empezar con los 4 primeros ataques; el rastro si sobra espacio.
Evoluciones (combinar dos mejoras en una más fuerte): para una segunda versión.

## Lo que necesita la base
- **Guardar un bloque de estado** (binario o hex) para la partida a medias; al leerlo,
  validarlo y, si viene mal, descartarlo y empezar una partida nueva. Le serviría a
  cualquier juego largo.

## Encaje con los principios
- **Discreto:** todo se mueve solo mientras juegas; en reposo y en pausa, quieto; sin
  sonido ni adornos.
- **Liviano:** el widget que más trabaja mientras se juega; pone a prueba los topes
  (0 en reposo, ≤ 3 ms por frame), no los rompe.
- **Justo:** sin progreso permanente, nada aparece de sorpresa, las pausas no castigan.
- **Claro:** formas como segunda señal, tarjetas cortas, récord de una sola cifra.
- **Coherente:** paleta, menú, pausa y foco como los demás.
