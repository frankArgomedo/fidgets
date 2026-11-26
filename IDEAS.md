# Ideas para nuevos widgets

Banco de ideas, sin compromiso. Cada una ya se pensó contra las reglas de `CLAUDE.md`;
al empezar una, su ficha se escribe en el `.c` y la idea sale de esta lista.

## Qué es un fidget

Una actividad de segundos que **se empieza con un gesto, se abandona gratis, se entiende
con opacidad baja y en reposo no cuesta nada**.

- Llena un hueco de segundos o un par de minutos; una ronda es corta.
- Dejarlo no cuesta nada: ocultar, perder el foco o suspender nunca castigan.
- Se lee de un vistazo: un dato grande, un aviso corto, nada más.
- En reposo, `fps = 0`; celda chica.
- Deja una huella pequeña: un resultado persistente y un récord ganado limpio.

Ejes que los distinguen: **tipo** (medición / juego / otro), **entrada** (mouse /
teclado) y **ritmo** (tiempo real / por turnos).

Hoy: `cube` (medición, teclado, tiempo real), `typing` (medición, teclado, tiempo
real), `dot` (juego, mouse, tiempo real), `keys` (otro, acompañante). Falta: puzles por
turnos, «otro» independiente, memoria y cálculo.

## Juguetes (tipo «otro»)

- **`pop`**: rejilla de burbujas que se revientan con clic; al reventarlas todas, se
  rellena. Sin récord ni estados; 0 en reposo. Celda ~120×80.
- **`spin`**: spinner que se arrastra para darle vuelo y gira con inercia hasta parar;
  récord de segundos girando. Necesita arrastre dentro del widget en la base
  (pendiente).
- **`glass`**: reloj de arena / temporizador (5, 15, 25 min). Es el caso de sonido que
  permite `CLAUDE.md` (temporizador que termina, `widget_beep`, pendiente en la base).
  Puede redibujar a 1 fps mientras corre.

## Mediciones

- **`react`**: tiempo de reacción. Esperar, cambia la señal, clic; mediana de 5
  intentos. Clic antes de tiempo = inválido (en `error`). El cambio necesita una
  segunda señal además del color («¡ya!»). Celda ~160×120.
- **`span`**: memoria de dígitos. Se muestra una secuencia y se escribe; récord = la
  más larga repetida. Teclado, por turnos.
- **`sums`**: cálculo mental, aciertos en 60 s. Modelo de `typing` (teclado numérico,
  récord por dificultad).
- **`pulse`**: mantener un ritmo con espacio contra un metrónomo visual; resultado =
  desviación en ms.

## Puzles por turnos

Sin reloj: no hace falta pausa ni aviso de foco.

- **`slide`**: puzle deslizante 3×3 / 4×4 (cabe en 120×120); récord de movimientos o
  tiempo por tamaño.
- **`flip`**: apagar luces 5×5; cada clic invierte la casilla y sus vecinas. Solo mouse.
- **`pix`**: nonogramas 5×5 generados al azar; comprobar que la solución sea única
  (puede ser una herramienta de desarrollo).
- **`sweep`**: buscaminas. Problema: el clic derecho está reservado al menú; marcar
  minas necesita un modo aparte o mantener pulsado.

## Tiempo real con teclado

- **`worm`**: la serpiente clásica en ~160×160. Se pausa al perder el foco.

## Acompañantes

- **`cube --stats`**: ao5 / ao12 con las últimas 12 resoluciones (ya se guardan en
  `t0…t11`, no se muestran).
- **`typing --keys`**: mapa de calor de las teclas con más errores; reusa el dibujo de
  `keys`.

## Por dónde empezar

1. `pop`: el fidget más puro, casi sin riesgo; prueba la base barato.
2. `react`: medición nueva con un patrón ya probado.
3. `slide`: estrena los puzles por turnos.

## Al elegir nombres

Una palabra en inglés que no choque con un comando de Windows (evitar `sort`, `tree`,
`time`, `find`, `type`…). Solo mecánicas genéricas; nada del nombre ni el aspecto de un
juego concreto.
