# swarm: survivors-like de bolsillo

Nombre provisional (`swarm` u `horde`; ninguno choca con un comando de Windows).
Estado: **en diseño**, con prototipo en `widgets/swarm/swarm.c` (paso 2 hecho).

Género: *survivors-like* (también *bullet heaven*). Se toman sus mecánicas (ataque
automático, oleadas que crecen, elegir una mejora al subir de nivel); nunca el nombre,
las armas, los personajes ni el aspecto de un juego concreto.

## Probar el prototipo

No va en los releases (`widgets/swarm/PROTOTIPO`) ni en los README. En cualquier máquina
con el repo y [Zig](https://ziglang.org/) (`winget install zig.zig`):

```powershell
.\build.ps1 swarm
$env:SWARM_LOG = "$env:APPDATA\fidgets\swarm_partidas.txt"; .\bin\swarm.exe
```

Con `SWARM_LOG`, cada partida terminada (y cada formación) se anota en ese archivo; sin
la variable no escribe nada. El simulador: `.\herramientas\simular.ps1` (ver
«Simulación»).

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
mientras haya cupo. Con todo al máximo ya no hay tarjetas ni cura: empieza la recta
final (ver «Curva de dificultad»).

| Ataque | Nivel 1 | Nv 2 | Nv 3 | Nv 4 | Nv 5 |
|---|---|---|---|---|---|
| **Disparo**: cada proyectil a un enemigo distinto (el más cercano, el segundo…) en la vista; 180 px/s | 1 proyectil, daño 10, cada 1,0 s | 2 proyectiles | daño 15 | atraviesa 1 enemigo | 3 proyectiles, cada 0,8 s |
| **Aura**: daña a todo lo que está dentro | radio 24 px, 5 de daño cada 0,5 s | radio 30 | daño 7 | radio 36 | daño 9 y frena 30 % |
| **Órbita**: bolas de 8 px girando a tu alrededor | 1 bola a 32 px, una vuelta cada 2 s, daño 10 | 2 bolas | daño 14 | 3 bolas | a 40 px, una vuelta cada 1,5 s |
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

### Enemigos
Mismas unidades: el jugador empieza con 100 de vida y 70 px/s; vista de ~114 px.

| Tipo | Forma y tamaño | Vida | Velocidad | Daño | Gema | Desde |
|---|---|---|---|---|---|---|
| **Básico** | triángulo, 12 px | 10 | 40 px/s | 10 | 1 XP | min 0 |
| **Rápido** | triángulo estrecho que apunta hacia donde va, 10 px | 5 | 75 px/s (el tope) | 6 | 1 XP | min 1:30–2:30 |
| **Lento** | cuadrado, 14 px | 40 | 25 px/s | 15 | 3 XP | min 3:30–4:30 |
| **Jefe** | cuadrado grande con borde, 24 px | 300 | 35 px/s | 20 | gema grande: lo que falte para subir de nivel | min 5–7, y cada 2:30–3:30 |

- Cada tipo pide otra respuesta: el básico es el relleno; del rápido no se escapa sin
  la pasiva de velocidad, pero muere con cualquier cosa; el lento se esquiva fácil pero
  tapa el camino y aguanta; el jefe pone a prueba el daño de la build y siempre deja una
  subida de nivel.
- Con la curva: la **vida** la multiplica la curva (todos los tipos, también el jefe);
  la **velocidad** de básico y lento sube de a poco entre el min 4 y el 12 (básico
  40 → 55, lento 25 → 35 px/s; el rápido ya va al tope de 75); el **daño** sube +10 %
  cada 2 min, hasta ×2.
- **El lento y el jefe resisten:** el empuje de la onda y el frenado del aura les
  afectan la mitad (al jefe el empuje no le afecta).
- **Formaciones:** ver «Eventos al azar».

Proporción de tipos que aparecen:

| Minuto | Básico | Rápido | Lento |
|---|---|---|---|
| 0–2 | 100 % | — | — |
| 2–4 | 80 % | 20 % | — |
| 4+ | 60 % | 25 % | 15 % |

- Aparición: ~0,7 enemigos/s al empezar, subiendo hasta el tope de ~54 visibles hacia
  el min 4. Arreglo fijo de **64 enemigos** (los visibles más los que vienen llegando de
  fuera de la vista).
- Todos van derecho hacia el jugador y **se empujan un poco entre sí** para no apilarse
  en un punto (se ve cuántos hay; con 64 son ~2.000 comparaciones por frame).
- El que queda a más de ~170 px reaparece por delante: escapar corriendo no quita la
  presión.
- **Flecha al jefe:** mientras el jefe está fuera de la vista, una flecha en el borde de
  la ventana apunta hacia él (información: alfa ≥ 200). Que no aparezca de sorpresa.

### Gemas y experiencia
- Gemas: rombos de 6 px; caen donde muere el enemigo y vuelan hacia el jugador al entrar
  en el radio del imán. **No vencen nunca** (perderlas por no llegar a tiempo iría contra
  «Justo»).
- Tope de 32 gemas en memoria: con el tope lleno, la más lejana (la que quedó atrás)
  se suma a la nueva, que cae cerca del jugador. No se pierde experiencia y no queda
  lejos. La gema que acumula se ve más grande.
- El nivel *n* pide **5 + 4·(n−1)** XP (5, 9, 13, 17…).

| Hito | XP acumulada | Objetivo |
|---|---|---|
| Nivel 2 | 5 | ~10–15 s |
| Nivel 5 | 44 | ~1 min |
| Nivel 15 | 490 | ~4 min |
| Techo (34 subidas) | 2.414 | ~11–12 min |

- El techo llega más o menos cuando termina una buena partida: una normal termina con
  cupo libre, y curar 30 casi nunca se ve.
- Una tarjeta cada ~10–15 s al principio y cada ~25 s al final: las recompensas van
  al principio.
- La simulación comprueba estos hitos, junto con la mediana del bot y que los 4 inicios
  queden parejos.

### Accesibilidad
Las formas son la segunda señal además del color: jugador = círculo, gemas = rombos, y
cada tipo de enemigo su forma y tamaño (triángulo, triángulo estrecho, cuadrado, cuadrado
grande con borde).

### Curva de dificultad
La partida termina cuando la amenaza supera el poder del jugador. **El poder tiene
techo y la amenaza no**: cada mejora tiene nivel máximo y hay cupos (34 subidas hasta el
techo, ver «Ataques y pasivas»). Así
la partida es infinita en las reglas pero siempre termina, y lo que dura lo deciden
cómo juegas y qué eliges.

Qué sube:

| Qué | Tope | Por qué |
|---|---|---|
| Enemigos por segundo | Los ~54 visibles (15 % de cobertura) | Más no deja ver por dónde escapar |
| Velocidad de los enemigos | ~75 px/s | Límite para reaccionar a tiempo (básico y lento suben hasta 55 y 35) |
| Vida de los enemigos | **Sin tope** | Es lo que hace que la partida termine |
| Daño de los enemigos | Sube poco | Que un solo error no te mate |
| Tipos de enemigo y formaciones | En ventanas al azar | Cambian cómo juegas, no solo cuánto |

Vida de los enemigos: **×1,27 por minuto desde el min 2**, sin saltos (antes era +25 %
por minuto desde el min 4 y el doble cada 2 min desde el 8: daba una meseta cómoda y
después una pared). Ajustado con la simulación.

**Recta final:** cuando todo está al máximo (cupos llenos y nivel 5), subir de nivel ya
no cura y la vida de los enemigos pasa a subir **×1,5 por minuto**. Después del techo ya
no pasaba nada interesante y se podía seguir indefinidamente (17:21 y 14:56, «me dejé
morir»); ahora una buena partida termina 2–3 min después de llegar al techo, en una
pelea cada vez más difícil.

Fases:

| Minuto | Qué pasa | Para qué |
|---|---|---|
| 0–1 | Solo el básico, pocos y lentos; primera subida de nivel a los ~10–15 s | Quien juega 2 min ya eligió mejoras y sintió que crece: las recompensas van al principio |
| 1–4 | Más enemigos hasta el tope; llegan el **rápido y débil** y el **lento con mucha vida** | Se arma la build; cada tipo pide otra respuesta |
| 4–8 | Tope de cantidad; los enemigos aceleran; **jefes** y **formaciones** | La build se pone a prueba; cada formación pide otra respuesta |
| ~10+ | Las mejoras llegan al techo: **recta final** | Termina en 2–3 min, peleando |

### Eventos al azar
Lo que tenía fecha fija se volvía predecible (el anillo del min 6 sorprendía una vez).
**Las curvas siguen el reloj, igual para todos** (vida, velocidad, daño, ritmo de
aparición): así cada partida es igual de difícil en promedio y los récords se comparan.
**Los eventos caen al azar dentro de ventanas**, distintos en cada partida:

| Evento | Ventana |
|---|---|
| Llega el rápido | min 1:30–2:30 |
| Llega el lento | min 3:30–4:30 |
| Jefe | el primero entre el min 5 y el 7; después cada 2:30–3:30 |
| Formación | la primera entre el min 5 y el 7; después cada 35–60 s |
| Olas | 25–45 s fuertes y 8–12 s de respiro |

**Formaciones:** una al azar entre las desbloqueadas, nunca la misma dos veces seguidas.

| Formación | Desde | Qué es | Qué pide |
|---|---|---|---|
| **Anillo** | siempre | 15 enemigos en círculo fuera de la vista, con un hueco de ~60° | abrirse paso o salir por el hueco |
| **Estampida** | siempre | fila de 8 rápidos que cruza en línea recta por donde estás | apartarse a tiempo |
| **Muro** | min 7 | fila de 7 lentos que avanza en línea recta | rodearlo o romperlo |
| **Anillo doble** | min 9 | dos anillos con los huecos en lados opuestos | elegir por dónde salir |

- Desde el min 10, uno de cada tres enemigos de los anillos es lento.
- Las de línea recta no persiguen: cruzan y desaparecen al alejarse.
- **Aviso:** el anillo se ve venir (~2 s); la estampida y el muro arrancan lejos y se
  anuncian ~2 s antes con tres flechas en el borde, del lado por donde vienen. Las
  flechas de aviso (y la del jefe) van en `accent`, para no confundirse con enemigos.
- Si no hay lugar en memoria, se liberan los enemigos más lejanos.

Otros:
- **Jefe:** un enemigo grande (~24 px) con mucha vida, sin ataques especiales.
- **Sin enemigos que disparan** en la primera versión; pueden llegar después como un
  tipo nuevo.

Justo:
- La curva depende solo del **tiempo de juego** (el mismo del récord, que no corre en
  pausa) y es igual para todos: nada de dificultad que se adapta a cómo vas. Lo único
  al azar es cuándo y cuál evento, con el mismo reparto para todos.
- Ningún enemigo aparece a menos de ~100 px del jugador: aparecen fuera de la vista.
- Tras un golpe, ~0,5 s de invulnerabilidad (con su destello transitorio): no se muere
  de un solo golpe.
- Un **modo difícil** (la misma curva, más rápida) con su propio récord queda para
  después.

Cómo se ajusta: con la simulación (ver «Simulación»). Objetivo: mediana del bot
~4–5 min, para que una persona que piensa sus mejoras llegue a 6–10.

### Celda, cantidad de enemigos y tarjetas
Decidido con el prototipo (fotos a 200, 240 y 280, al 100 % y al 25 % de opacidad,
sobre fondo oscuro y claro).

- **Celda 240×240.** Es la menor que cumple el requisito de reacción: desde el borde
  hay ~114 px hasta el jugador, ~1,5 s para el enemigo rápido (75 px/s). En 200 son
  ~94 px (1,25 s) y las tarjetas no dan para «Velocidad»; en 280 sobra espacio en el
  inicio y los enemigos se ven chicos.
- **Enemigos:** tope de 54 visibles (15 % de cobertura con enemigos de 12 px) y 64 en
  memoria. Se ve lleno pero se lee.
  Fórmula: visibles ≈ cobertura × área útil ÷ área de un enemigo; con más de ~20 % ya
  no se ve por dónde escapar. Al llegar al tope, la presión sube con más vida, no con
  más cantidad.
- **3 tarjetas** de ~72 px al subir de nivel; la de inicio, 4 ataques en 2×2.
- **Sin panel en pausa ni al elegir:** el juego se atenúa (alfa /4), el fondo no.

Consumo con la pantalla llena (54 enemigos, 3 ataques; `consumo.ps1` con
`SWARM_FILL=1`): .exe 98,5 KB, memoria 1,79 MB, CPU en reposo 0 ms, dibujo 0,71 ms por
frame. Todo dentro de los topes.

### Simulación (paso 3)
`herramientas\simular.ps1 [partidas] [semilla] [ms de reacción]` compila
`herramientas\swarm_sim.c`, que incluye `swarm.c` tal cual (la misma lógica del widget),
y juega cientos de partidas sin ventana con dos bots que eligen tarjetas al azar:
- **jugador:** esquiva lo cercano, va por las gemas y, con un ataque de cerca, se acerca
  a los enemigos hasta tenerlos a tiro;
- **que solo huye:** nunca se acerca a nada (mide si escapar sin pelear alcanza).

**Calibración con partidas reales** (registro `SWARM_LOG`, eligiendo siempre la primera
tarjeta, que equivale a elegir al azar como el bot): disparo 5:06, aura 3:44, órbita
4:14, onda 9:46; mediana ~4:40. Caen entre el bot de 156 y el de 200 ms: el bot
decide cada **~175 ms** por defecto.

Lo que salió de la simulación y de jugar:
- **Hitos de experiencia:** se cumplen con cualquier bot: nivel 2 a ~0:10, nivel 5 a
  ~0:50, nivel 15 a ~3:40, techo a ~10 min.
- **Ataques iniciales parejados:** la órbita (daño 8 no mataba de un golpe al básico)
  y el aura eran los peores. Subidos: órbita 10 / 14, aura 5 / 7 / 9.
- **Huir sin pelear no alcanza** salvo con reflejos imposibles (~22 ms).
- **Desde el min ~10 no se sentía difícil** (eligiendo a gusto: 17:21 en nivel 37, «me
  dejé morir»): la cantidad dejaba de subir en el min 4, rodear al grupo funcionaba
  demasiado bien y la onda y el aura neutralizaban todo. Arreglos: anillos, enemigos
  que aceleran, resistencia del lento y el jefe, y la vida en una sola curva.
- **Las gemas quedaban lejos** (la sobrante se sumaba a la más lejana): ahora te siguen.
- **El disparo rendía poco al subir** (los proyectiles extra iban al mismo enemigo):
  ahora cada uno va a un enemigo distinto.

- **Después del techo no pasaba nada interesante** (con los arreglos: 14:56 en nivel 44,
  «me dejé morir»; los anillos se notaban la primera vez y después no): recta final,
  formaciones que rotan y eventos al azar.

Hoy (formaciones, recta final y eventos al azar), 100 partidas por inicio:

| Bot | Disparo | Aura | Órbita | Onda | Mediana | Tras el techo |
|---|---|---|---|---|---|---|
| ~175 ms (como una persona) | 5:19 | 5:04 | 4:54 | 4:24 | **5:00** | 0:52–2:00 |
| 44 ms (una muy buena partida) | 11:21 | 10:55 | 10:37 | 11:40 | **11:04** | 2:00–3:04 |
| ~175 ms, solo huye | | | | | 2:11 | |

Mediana en el borde alto del objetivo (4–5 min), inicios a menos de 20 % entre sí y una
buena partida termina 2–3 min después del techo.

El registro (`SWARM_LOG`) anota también el segundo del techo (`techo=`, -1 si no llegó)
y cada formación (`formacion=… segundo=…`).

## Por decidir

- Confirmar con `llenado.ps1` que el nombre más largo («Velocidad», «Fire rate») cabe
  en la tarjeta y los márgenes de cada estado, en los dos idiomas.
- Jugar otra tanda: ¿las formaciones sorprenden y se leen? ¿la recta final se siente
  como un cierre y no como un corte? ¿eligiendo a gusto llega a 6–10 min?

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
