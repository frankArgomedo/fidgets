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

Para probar una parte sin empezar de cero, `SWARM_START` con el minuto (hasta 30):
eliges el ataque y las mejoras que tendrías a esa altura (~1 cada 16 s) se eligen al
azar; la vida, la velocidad y los eventos son los de ese minuto (la primera formación a
los 5–15 s, el jefe a los 20–40 s). Desde el min 10 ya está todo al máximo: empieza en
la recta final. No cuenta para el récord y el registro lo anota (`desde_min=`).

```powershell
$env:SWARM_START = 5; $env:SWARM_LOG = "$env:APPDATA\fidgets\swarm_partidas.txt"; .\bin\swarm.exe
```

Para volver a empezar de cero en esa misma consola: `Remove-Item Env:SWARM_START`.

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
| **Jefe** | cuadrado grande con borde, 24 px | 300 | 35 px/s | 20 | gema grande: lo que falte para subir de nivel | min 3:30–4:30, y cada 1:30–2:30 |

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
  el min 4. Arreglo fijo de **96 enemigos** (los visibles, los que vienen llegando de
  fuera de la vista y una formación).
- Todos van derecho hacia el jugador y **se empujan un poco entre sí** para no apilarse
  en un punto (se ve cuántos hay; con 96 son ~4.500 comparaciones por frame).
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
- El nivel *n* pide **5 + 4·(n−1)** XP (5, 9, 13, 17…) **hasta un tope de 85** (desde
  el nivel 21). La experiencia que entra deja de crecer hacia el min 5 (la aparición
  llega a su tope); sin el tope, las últimas tarjetas se espaciaban hasta ~30 s («siento
  que es algo más lento al final»). Con él, una cada ~15–18 s hasta el techo.

| Hito | XP acumulada | Objetivo |
|---|---|---|
| Nivel 2 | 5 | ~10–15 s |
| Nivel 5 | 44 | ~1 min |
| Nivel 15 | 490 | ~4 min |
| Techo (34 subidas) | 2.050 | ~9 min |

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
no cura, la vida de los enemigos pasa a subir **×1,5 por minuto** y su velocidad **+10 %
por minuto** (el rápido alcanza tu velocidad máxima a los ~3 min). Solo con la vida, una
build que esquiva y regenera (velocidad y vida al máximo, sin daño) aguantaba 5:20
esquivando («me dejé morir porque era largo»). Después del techo ya
no pasaba nada interesante y se podía seguir indefinidamente (17:21 y 14:56, «me dejé
morir»); ahora una buena partida termina 2–3 min después de llegar al techo, en una
pelea cada vez más difícil.

Fases:

| Minuto | Qué pasa | Para qué |
|---|---|---|
| 0–1 | Solo el básico, pocos y lentos; primera subida de nivel a los ~10–15 s | Quien juega 2 min ya eligió mejoras y sintió que crece: las recompensas van al principio |
| 1–4 | Más enemigos hasta el tope; llegan el **rápido y débil** y el **lento con mucha vida** | Se arma la build; cada tipo pide otra respuesta |
| 2:30–4 | Primeras **formaciones** (anillo, estampida) | Se resuelven moviéndote, no con daño: llegan aunque la build sea débil |
| 4–8 | Tope de cantidad; los enemigos aceleran; **jefes**; muro (5) y anillo doble (7) | La build se pone a prueba; cada formación pide otra respuesta |
| ~9+ | Las mejoras llegan al techo: **recta final** | Termina en 2–3 min, peleando |

### Eventos al azar
Lo que tenía fecha fija se volvía predecible (el anillo del min 6 sorprendía una vez).
**Las curvas siguen el reloj, igual para todos** (vida, velocidad, daño, ritmo de
aparición): así cada partida es igual de difícil en promedio y los récords se comparan.
**Los eventos caen al azar dentro de ventanas**, distintos en cada partida:

| Evento | Ventana |
|---|---|
| Llega el rápido | min 1:30–2:30 |
| Llega el lento | min 3:30–4:30 |
| Jefe | el primero entre el min 3:30 y el 4:30 (después de la primera formación); después cada 1:30–2:30 (~5 en una buena partida; si no matas uno a tiempo, el siguiente lo alcanza) |
| Formación | la primera entre el min 2:30 y el 3:30; después cada 35–60 s |
| Olas | 25–45 s fuertes y 8–12 s de respiro |

**Formaciones:** una al azar entre las desbloqueadas, nunca la misma dos veces seguidas.

| Formación | Desde | Qué es | Qué pide |
|---|---|---|---|
| **Anillo** | siempre | 15 enemigos en círculo fuera de la vista, con un hueco de ~60° | abrirse paso o salir por el hueco |
| **Estampida** | siempre | franja de dos filas de 22 rápidos (~350 px, más ancha que la vista; la segunda 14 px atrás y corrida medio lugar) que cruza en línea recta por donde estás, con un hueco de 3 | llegar al hueco a tiempo (2 s de aviso) |
| **Muro** | min 5 | fila de 32 lentos (~510 px) con **el triple de vida**, que avanza en línea recta a 1,4 veces el paso del lento (~37–49 px/s), con un hueco de 3 cerca del centro; **si vas hacia una punta, se corre de costado** (hasta 65 px/s) para quedar frente a ti | buscar el hueco o romper la pared: rodearlo ya no se puede |
| **Anillo doble** | min 7 | dos anillos con los huecos en lados opuestos | elegir por dónde salir |

- Desde el min 10, uno de cada tres enemigos de los anillos es lento.
- **Muro que se corre de lado:** a menos de 100 px de su centro (donde cae el hueco, a
  menos de 40 px) no se mueve de costado y llegar al hueco no cuesta; más allá se corre
  hacia ti a 65 px/s. Ni con la velocidad al máximo (98) se rodea: desde los 100 px
  ganas 33 px/s y faltan ~165 en los ~5 s que tarda en llegar. Con 510 px recto y sin
  correrse «aún lo pude rodear». Se eligió frente a un muro en arco (sin perseguir,
  pero rodeable corriendo hacia atrás y parecido al anillo), más largo (~1.000 px, no
  cabe en memoria) y una pinza de dos muros (se escapa justo; se parece al anillo doble).
- Las de línea recta no persiguen (salvo ese corrimiento del muro): cruzan y
  desaparecen al alejarse. Nada las empuja
  (ni los otros enemigos ni la onda): la fila no se deforma y aparta a los demás.
- El hueco cae al azar cerca del medio. Antes eran de ~100 px y pasaban lejos si te
  movías («las vi pasar muy lejos»); más anchas que la vista, te alcanzan siempre.
- **Despeje:** 3 s antes de cada formación no aparecen enemigos sueltos, para que llegue
  con la pantalla más vacía y se lea como un evento (con 54 en pantalla, 8 más no se
  notaban).
- **Aviso:** el anillo se ve venir (~2 s); la estampida y el muro arrancan lejos y se
  anuncian ~2 s antes con tres flechas en el borde, del lado por donde vienen. Las
  flechas de aviso (y la del jefe) van en `accent`, para no confundirse con enemigos.
- Si no hay lugar en memoria, se liberan los enemigos más lejanos.

Otros:
- **Jefe:** un enemigo grande (~24 px) con mucha vida y **embestida**: cada ~4 s, si
  está a la vista, se para 0,8 s marcando con una línea de puntos `accent` hacia donde
  estabas, y se lanza en línea recta a 110 px/s (~3 veces su paso) durante 1,8 s
  (~200 px: arranca en el borde y se pasa de largo). Mientras embiste, nada lo empuja
  ni lo frena. Pide mirarlo y apartarse a tiempo; antes era un lento grande y «solo es
  tiempo» matarlo.
- **Fuera de la vista, te alcanza embistiendo:** no camina, encadena embestidas hacia
  ti con 0,3 s de pausa (~95 px/s de media, casi tu velocidad máxima: solo escapas
  huyendo en línea recta con la velocidad al máximo). La flecha del borde late (más
  grande) con cada embestida. Al entrar en la vista corta la embestida y vuelve a
  caminar; nunca entra embistiendo sin la línea de aviso.
  Antes te seguía a 35 px/s y con velocidad alta quedaba lejos («si escapaba los dejaba
  muy lejos», 3 jefes a la vez sin enfrentarlos). Se descartaron: reaparecer por delante
  como los demás (un jefe es único: «se teletransporta») y correr más rápido fuera de la
  vista («medio raro»).
  Otros juegos del género no teletransportan a sus jefes: te persiguen por el mapa y
  tienen cómo alcanzarte (un salto), o el mapa es cerrado.
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
- **Enemigos:** tope de 54 visibles (15 % de cobertura con enemigos de 12 px) y 96 en
  memoria (los visibles, los que llegan y una formación entera). Se ve lleno pero se lee.
  Fórmula: visibles ≈ cobertura × área útil ÷ área de un enemigo; con más de ~20 % ya
  no se ve por dónde escapar. Al llegar al tope, la presión sube con más vida, no con
  más cantidad.
- **3 tarjetas** de ~72 px al subir de nivel; la de inicio, 4 ataques en 2×2.
- **Sin panel en pausa ni al elegir:** el juego se atenúa (alfa /4), el fondo no.

Consumo con la pantalla llena (54 enemigos, 3 ataques; `consumo.ps1` con
`SWARM_FILL=1`): .exe 101 KB, memoria 1,79 MB, CPU en reposo 0 ms, dibujo 0,51–0,74 ms
por frame (con 96 en memoria, igual que con 64). Todo dentro de los topes.

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
  ~0:50, nivel 15 a ~3:40, techo a ~10 min (~9 min con el tope de experiencia).
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

Con estampida y muro grandes y el despeje: mediana **4:39** (disparo 5:03, aura 4:41,
órbita 4:44, onda 4:22); solo huye 2:06.

- **Formaciones que no se notaban** (2 partidas: 5:25 y 13:32, «me mataron», la recta
  final «lo más apretado» pero sin corte de golpe): estampida y muro más anchos que la
  vista, con hueco, que no se deforman, y despeje antes de cada formación.

**Ritmo de las tarjetas** (bot de 44 ms para llegar al techo; mediana del segundo en que
sale cada una). Antes del tope de experiencia, las últimas se espaciaban: +21 s en el
nivel 28, +25 s en el 32, ~30 s cerca del techo (10:15). Con el tope de 85:

| Nivel | 2 | 3 | 4 | 5 | 8 | 12 | 16 | 20 | 24 | 28 | 32 | 35 (techo) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Segundo | 10 | 20 | 31 | 48 | 96 | 164 | 234 | 299 | 367 | 431 | 498 | 548 |
| Espera | 10 | 10 | 11 | 17 | 19 | 16 | 18 | 16 | 16 | 15 | 18 | 18 |

Las tres primeras salen cada ~10 s y después cada ~15–18 s hasta el techo. Tus tiempos
reales (13, 19, 32, 42, 63, 76, 98 s) coinciden con el bot.

- **Formaciones tarde:** con la primera en el min 5–7, la mitad de las partidas
  terminaba sin ver ninguna (las 3 tuyas). Ahora la primera llega en el min 2:30–3:30
  (muro desde el 5, anillo doble desde el 7). Sola, no cambia la mediana (4:40).
- Probado y descartado: pedir más experiencia al inicio para que todas las tarjetas
  salieran cada ~18 s desde la primera. La build llegaba débil a la mitad (nivel 15 a
  4:18 en vez de 3:37) y la mediana bajaba a 3:25.

Hoy (formaciones desde el min 2:30 y tope de experiencia), 100 partidas por inicio:

| Bot | Disparo | Aura | Órbita | Onda | Mediana | Tras el techo |
|---|---|---|---|---|---|---|
| ~175 ms (como una persona) | 5:15 | 4:42 | 4:41 | 4:48 | **4:50** | 1:47–2:26 |
| 44 ms (una muy buena partida) | 10:46 | 11:44 | 11:24 | 11:37 | **11:16** | 2:50–3:56 |

- **Jefe sin desafío y tarde** («es fácil matarlo, solo es tiempo»; con el primero en
  el min 5–7, una partida normal no veía ninguno): embestida y primer jefe en el min
  3:30–4:30. Con eso (el bot no lee el aviso; esquiva solo lo que tiene cerca):

| Bot | Disparo | Aura | Órbita | Onda | Mediana | Tras el techo |
|---|---|---|---|---|---|---|
| ~175 ms (como una persona) | 4:36 | 4:21 | 4:15 | 4:17 | **4:22** | 1:16–2:14 |
| 44 ms (una muy buena partida) | 10:41 | 10:01 | 10:52 | 11:30 | **10:55** | 2:43–3:33 |

Con jefes cada 2:30–3:30 una buena partida veía ~3; ahora cada 1:30–2:30 (~5):

| Bot | Disparo | Aura | Órbita | Onda | Mediana | Tras el techo |
|---|---|---|---|---|---|---|
| ~175 ms (como una persona) | 4:51 | 4:37 | 4:19 | 4:47 | **4:35** | 1:46–2:12 |
| 44 ms (una muy buena partida) | 10:41 | 10:49 | 10:28 | 11:09 | **10:47** | 2:55–3:26 |

- **Final largo, jefes que quedaban atrás y muro fácil de rodear** (15:24, techo en el
  10:04, «me dejé morir porque era largo»): velocidad que sube en la recta final, jefe
  que te alcanza embistiendo y muro de ~510 px más rápido.

| Bot | Disparo | Aura | Órbita | Onda | Mediana | Tras el techo |
|---|---|---|---|---|---|---|
| ~175 ms (como una persona) | 4:55 | 4:33 | 4:21 | 4:19 | **4:29** | 1:41–2:18 |
| 44 ms (una muy buena partida) | 9:52 | 8:59 | 10:45 | 11:12 | **10:31** | 2:21–2:41 |

- **Estampida que se veía chica y muro que se rodeaba** (desde el min 5 con
  `SWARM_START`: el jefe «volvía embistiendo», «el muro aún lo pude rodear», la
  estampida «es pequeña»): estampida en doble fila y muro que se corre de lado.

| Bot | Disparo | Aura | Órbita | Onda | Mediana | Tras el techo |
|---|---|---|---|---|---|---|
| ~175 ms (como una persona) | 4:43 | 4:18 | 4:13 | 4:38 | **4:32** | 1:29–2:15 |
| 44 ms (una muy buena partida) | 9:50 | 10:19 | 10:17 | 10:54 | **10:25** | 2:20–2:54 |

El registro (`SWARM_LOG`) anota también el segundo del techo (`techo=`, -1 si no llegó),
cada formación (`formacion=… segundo=…`) y el segundo de cada tarjeta (`tarjetas=…`,
desde el nivel 2).

## Por decidir

- Confirmar con `llenado.ps1` que el nombre más largo («Velocidad», «Fire rate») cabe
  en la tarjeta y los márgenes de cada estado, en los dos idiomas.
- Probado (1 partida, 5:28, «en general es más divertido»): la línea de puntos del jefe
  se ve a tiempo y la estampida se lee («me metí por el hueco»).
- Probado: el jefe que te alcanza embistiendo funciona («me podía librar un poco de la
  presión por un momento, pero no quedaba lejos»).
- Probar: ¿el muro que se corre de lado se siente justo o tramposo? ¿la estampida en
  doble fila se nota? ¿la recta final cierra en 2–3 min aunque la build sea de esquivar?
- **Jefe:** si la embestida no alcanza para hacerlo desafiante, una escolta (anillo de 8
  básicos al bajar a la mitad de vida).
- **Tipos de jefe** (para después): cada tipo con otro comportamiento y otra forma
  (segunda señal, no solo color), la misma dificultad (la del reloj) y desbloqueados con
  el tiempo como las formaciones. Sin niveles de dificultad por jefe (habría que leerlos
  de un vistazo). Idea: uno que **salta** y cae cerca de ti, con un círculo de aviso
  ~1 s antes.

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
