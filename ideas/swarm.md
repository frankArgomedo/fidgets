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
triángulos, gemas = rombos.

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

### Curva de dificultad
Ajustarla para que una partida normal dure unos 5–10 min. Falta definir qué sube y
cada cuánto (aparición, vida, velocidad, tipos).

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
