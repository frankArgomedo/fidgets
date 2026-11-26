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

### Ataques y pasivas
Unidades: vista de ~114 px hasta el borde, enemigos ≤ 75 px/s, enemigo básico con
10 de vida al empezar. Números de partida: los ajusta la simulación.

**Cupos:** hasta 3 ataques de 4 y 4 pasivas de 6, para que cada partida deje algo
afuera y elegir importe. 7 cosas × 5 niveles = 35 niveles; el primer ataque empieza en
nivel 1, así que son **34 subidas hasta el techo**.

**Ataque inicial: lo eliges tú.** La partida empieza con una tarjeta de 4 opciones (en
2×2), una por ataque, que se elige con clic o con 1–4. Es el único gesto para empezar
(«Aviso para empezar») y el clic le da el teclado al widget.
- Un solo récord: es una decisión dentro de la partida, como las demás tarjetas, no un
  ajuste del menú que cambie la dificultad (la regla de un récord por combinación de
  ajustes no aplica).
- Por eso mismo, los 4 inicios deben ser parejos: la simulación corre el bot con cada
  uno y sus medianas no deberían separarse más de ~20 %. Si uno gana siempre, se
  ajustan sus números.
- Cada ataque tiene que servir solo durante el primer minuto.

**Tarjetas:** mejoras distintas al azar, entre subir algo que tienes y algo nuevo
mientras haya cupo. Con todo al máximo, la tarjeta solo **cura 30**.

| Ataque | Nivel 1 | Nv 2 | Nv 3 | Nv 4 | Nv 5 |
|---|---|---|---|---|---|
| **Disparo**: proyectil al enemigo más cercano en la vista; 180 px/s | 1 proyectil, daño 10, cada 1,0 s | 2 proyectiles | daño 15 | atraviesa 1 enemigo | 3 proyectiles, cada 0,8 s |
| **Aura**: daña a todo lo que está dentro | radio 24 px, 4 de daño cada 0,5 s | radio 30 | daño 6 | radio 36 | daño 8 y frena 30 % |
| **Órbita**: bolas de 8 px girando a tu alrededor | 1 bola a 32 px, una vuelta cada 2 s, daño 8 | 2 bolas | daño 12 | 3 bolas | a 40 px, una vuelta cada 1,5 s |
| **Onda**: anillo que crece y empuja | cada 4 s, hasta 60 px en 0,3 s, daño 12, empuja 20 px | hasta 72 px | cada 3 s | daño 18 | hasta 84 px, empuja 32 |

- Cada uno resuelve otra cosa: el disparo mata de a uno y desde lejos, el aura limpia
  lo que se te pega, la órbita protege el anillo de en medio y la onda te saca cuando
  te rodean. El nivel 5 de cada uno cambia cómo se juega.
- Órbita y aura golpean a un mismo enemigo como máximo una vez cada 0,5 s (el daño no
  depende de los fps).
- El rastro y las evoluciones quedan para la segunda versión.

| Pasiva | Por nivel | Al nivel 5 | A qué se aplica |
|---|---|---|---|
| **Daño** | +10 % | +50 % | todos los ataques |
| **Cadencia** | −8 % de espera | −40 % | todo lo que se repite (disparo, golpes del aura, onda, vuelta de la órbita) |
| **Alcance** | +10 % de radio | +50 % | aura, órbita, onda y tamaño del proyectil |
| **Velocidad** | +8 % | 70 → ~98 px/s | tu movimiento |
| **Imán** | +12 px | 16 → 76 px | radio para recoger gemas |
| **Vida** | +20 máx. y +0,3/s de regeneración | 200 máx., 1,5/s | tú (100 al empezar; el básico quita 10 al tocarte) |

Tensión buscada: empiezas a 70 px/s y el enemigo rápido va a 75; sin la pasiva de
velocidad no puedes escapar de él, solo matarlo (tiene poca vida). Con nivel 2, sí.

En las tarjetas: nombre (≤ 10 caracteres en los dos idiomas), un ícono de ~24 px
dibujado con figuras (10 en total, segunda señal además del nombre) y el nivel como
5 puntitos (no depende del idioma).

| Español | English |
|---|---|
| Disparo | Shot |
| Aura | Aura |
| Órbita | Orbit |
| Onda | Wave |
| Daño | Damage |
| Cadencia | Fire rate |
| Alcance | Area |
| Velocidad | Speed |
| Imán | Magnet |
| Vida | Health |

En el juego, cada ataque tiene su forma: punto, anillo tenue, bolas, anillo que crece.

### Accesibilidad
Las formas son la segunda señal además del color: jugador = círculo, enemigos =
triángulos, gemas = rombos; el jefe se distingue por el tamaño.

### Curva de dificultad
La partida termina cuando la amenaza supera el poder del jugador. **El poder tiene
techo y la amenaza no**: cada mejora tiene nivel máximo y hay cupos (34 subidas hasta el
techo, ver «Ataques y pasivas»); con todo al máximo, subir de nivel solo cura 30. Así
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
La tarjeta inicial de 4 ataques va en 2×2, aparte de esta decisión; también se
comprueba con fotos.

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
