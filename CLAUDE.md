# fidgets

Colección de widgets para pasar el rato (minijuegos y otras cosas) que se quedan
en pantalla de forma discreta: pequeños, sin bordes, con transparencia y con el
menor consumo posible. Solo para Windows. El nombre juega con *fidget* (juguetear
para matar el rato) y *widgets*.

## Restricciones (no se negocian)

- Solo Windows.
- **C11 + Win32 + GDI**, sin dependencias ni runtimes en el producto (.NET, Electron,
  Python…). Las herramientas de desarrollo que generan datos pueden usar otro lenguaje
  (p.ej. `generar.py`), pero compilar y usar los widgets nunca las necesita.
- Cada widget es un .exe independiente: se instala y se comparte por separado.

## Principios (en orden de prioridad; el de arriba gana si dos chocan)

1. **Discreto:** nunca estorba ni llama la atención sin que lo pidas. Sin bordes ni
   panel, transparente, sin sonido por defecto, no roba el foco, Esc no cierra,
   ocultar no cuesta nada.
2. **Liviano:** el menor consumo posible de CPU, memoria y disco; «lo más liviano que
   siga siendo justo». `fps = 0` en reposo, topes medidos, nada por frame.
3. **Justo:** la interfaz nunca te hace perder; un resultado vale solo si se ganó
   limpio. Ocultar, perder el foco o suspender la PC no castigan.
4. **Claro:** se entiende de un vistazo, sin manual, con opacidad baja y en cualquier
   idioma.
5. **Coherente:** todos los widgets se ven y se usan igual; lo común vive en la base.

Cómo desempatan: un widget liviano que molesta no sirve (1 > 2); los contadores van a
10 fps para que cada número salga a tiempo (3 pesa sobre un costo mínimo de 2); una
medición sin teclado sigue corriendo con aviso en vez de detenerse (3 > 4); una
excepción declarada en la ficha puede hacer un widget más claro aunque rompa la
regla común (4 > 5).

## Compilar y ajustes

- `zig cc` (winget `zig.zig`). `.\build.ps1` compila todo; `.\build.ps1 <nombre>` uno.
  Salida en `bin/`. Muestra el tamaño de cada .exe y avisa si pasa el tope (128 KB).
- Ajustes: `%APPDATA%\fidgets\<exe>.ini`, una sección por widget (`FIDGETS_INI=ruta`
  la reemplaza en pruebas).

## Estructura

- `common/widget.h|.c`: la base común (ventana layered, menú, atajos, .ini, dibujo,
  idiomas). Toda la funcionalidad compartida va aquí, no se copia en cada widget.
- `widgets/<nombre>/*.c`: un widget por carpeta; el nombre de la carpeta es el del .exe.
  Cada widget define un `WidgetDesc` y llama a `widget_run`.
- Acompañante: lo que solo tiene sentido con otro widget vive dentro de su .exe, tiene
  su propio `name`, se abre con `--<name>` (`cube.exe --keys`, `widgets/cube/keys.c`),
  su sección vive en el .ini del principal, lo abre y cierra el principal desde su
  menú y se cierra con él.
- Datos grandes (listas, textos) en archivos junto al widget, incluidos con `#embed`.
- `IDEAS.md`: banco de ideas para nuevos widgets (no va en los releases). Las ya
  desarrolladas tienen su archivo en `ideas/<nombre>.md`. Al empezar un widget, su
  idea sale de ahí y lo vigente pasa a la ficha.
- Comunicación entre widgets: `widget_post(L"nombre", code, value)` (no bloquea; no
  hace nada si el otro no está abierto) y `on_message` en el receptor. Los códigos
  `MSG_…` van en el encabezado que comparten (p.ej. `widgets/cube/keymap.h`).

## Reglas para widgets

### Lo que ya da la base (no reimplementar)

Arrastrar (y Ctrl+arrastrar siempre), imán (Shift lo evita), el menú del clic derecho
(encima de todo, atenuar, opacidad, idioma, ocultar, cerrar), clic central para
cerrar, Ctrl+Alt+B ocultar/mostrar todos, Ctrl+Alt+Q cerrar todos, una sola
instancia, ajustes, idiomas y «Controles ▸».

### Ciclo de vida

Cada widget declara en su ficha su **tipo**: *medición* (produce un tiempo o una
velocidad que va a récord: `cube`, `typing`), *juego* (`dot`, puzles) u *otro*.

- **Ocultar, suspender o bloquear la sesión:** la base llama a `on_hide(1)` al
  ocultar, al suspender la PC (`WM_POWERBROADCAST`) y al bloquear la sesión
  (`WTSRegisterSessionNotification`), y `on_hide(0)` al volver. `on_hide` significa
  «no se puede usar».
  - Medición: se cancela sin penalizar; vuelve a como estaba antes de empezar y no
    guarda nada.
  - Juego: se pausa; al volver muestra «en pausa · clic para seguir» y solo sigue con
    ese clic.
  - Efectos transitorios: saltan a su final.
- **Foco del teclado** (la base nunca lo roba; se da con un clic):
  - Los widgets que usan el teclado muestran si lo tienen, en cada estado persistente,
    y el aviso común «clic para …» cuando no.
  - Juego que avanza solo y se juega con el teclado: se pausa al perder el foco
    («en pausa · clic para seguir»; ese clic le devuelve el teclado).
  - Medición: sigue corriendo, con aviso `accent` «sin teclado · clic para seguir»;
    Esc la anula.
  - Todo lo demás (se juega con el mouse, por turnos sin reloj, solo se mira): nada.
- **Instancias:** una por `name`, siempre (mutex `fidgets.<name>`). Abrirlo de nuevo
  lo trae al frente y lo muestra si estaba oculto.
- **Cerrar:** ajustes y récords ya están guardados; una medición a medias se pierde;
  un juego a medias se guarda solo si una partida suele durar más de un minuto, y al
  reabrir queda en «en pausa · clic para seguir».
- **Tiempo:** medir siempre con `now_seconds()` (no cambia con la hora del sistema);
  un widget que muestre la hora redibuja con `WM_TIMECHANGE`.

### Entradas

- **Mouse reservado:** clic derecho (menú), clic central (cerrar), Ctrl+arrastre
  (mover). El widget usa el clic izquierdo en sus zonas activas; `on_mouse` devuelve
  0 en zonas vacías para que se pueda arrastrar la ventana.
- **Rueda:** libre para el widget (la opacidad solo va en el menú), únicamente para
  cosas que no hagan daño si se tocan sin querer (desplazar, elegir un valor); nunca
  para empezar, cancelar o borrar.
- **Arrastre dentro del widget:** solo si `on_mouse` tomó el `MOUSE_DOWN`; entonces
  recibe `MOUSE_MOVE` hasta soltar.
- **Doble clic:** opción `.dblclick = 1`, desactivada por defecto (sin ella todos los
  clics llegan como clics normales). Si se usa, continúa lo que hizo el primer clic,
  nunca lo contradice, para no tener que esperar.
  (Arrastre y doble clic se programan en la base cuando un widget los necesite.)
- **Teclado reservado:** Ctrl+Alt+B, Ctrl+Alt+Q, combinaciones con Alt o Win (son de
  Windows; Alt+F4 cierra el widget) y F10. Todo lo demás es libre.
- **Esc** significa «cancelar lo que está en curso» y nunca otra cosa; NUNCA cierra un
  widget (se pulsa sin querer).
- `on_key` recibe el scan code físico (independiente de la distribución del teclado);
  las teclas extendidas (flechas, Insert, Supr, Inicio, Fin, RePág/AvPág, Enter
  numérico, Ctrl y Alt derechos) llegan con 0x100 sumado. Para escribir texto,
  `on_char` (carácter ya compuesto: tildes, ñ, mayúsculas).

### Dibujo y colores

- Sin panel de fondo por defecto: `gfx_clear_hit()` deja el fondo invisible pero
  agarrable con cualquier opacidad. Los píxeles con alfa final 0 dejan pasar el clic.
- Texto siempre con `| TEXT_SHADOW`. `TEXT_MONO` = Consolas. Un aviso que puede no
  caber a lo ancho (p.ej. los comunes de la base en un widget de 160 px) se dibuja con
  `gfx_text_wrap`, que lo parte en dos líneas en « · ».
- La opacidad (solo desde el menú) se aplica a todo el widget; el usuario la tiene
  casi siempre baja (~25 %) y a veces la sube.
- **Paleta común** `theme->…`, nunca colores sueltos para la interfaz:
  `text`, `sub`, `accent`, `error`, `extra` (texto y avisos); `surface` (relleno de
  teclas y controles), `surface_alt` (segunda categoría de control), `line`
  (separadores y bordes), `ok` (acierto, éxito, «jugar»; verde azulado), `ink`
  (ícono o texto sobre un relleno de color).
- **Colores del juego** (representan algo del juego o del mundo real, p.ej. las caras
  del cubo): constantes con nombre en el widget, listadas en la ficha; nunca para
  texto ni avisos. Si no representa algo del juego, viene de la paleta.
- Variantes (otra opacidad, mezcla) con las funciones de la base (`mix`, cambio de
  alfa), no con colores nuevos.
- **Accesibilidad:**
  - Si un color diferencia algo que importa para decidir, necesita una segunda señal
    (forma, texto, posición, subrayado).
  - Lo que lleva información usa alfa ≥ 200 antes de la opacidad; lo decorativo puede
    ser más tenue y desaparecer con opacidad baja.
  - Con `widget_reduce_motion()` = 1 (ajuste de animaciones de Windows), los efectos
    transitorios saltan a su final; la mecánica del juego no cambia.
  - Fuera de alcance: lectores de pantalla y usar un widget sin la entrada que su
    mecánica exige.

### Animación y consumo

- `fps = 0` siempre que no haya animación; `widget_redraw()` cuando cambie el estado.
  Nada de memoria asignada por frame.
- Mientras algo se mueve, solo estas constantes (nunca un número suelto en
  `widget_set_fps`, salvo 0):
  - `FPS_MOVE` 45: movimiento que la vista sigue (giros, deslizamientos).
  - `FPS_EFFECT` 30: efectos (cambio de tamaño o color, destellos).
  - `FPS_CLOCK` 20: cronómetro con centésimas corriendo.
  - `FPS_COUNTER` 10: contadores en segundos.
- Topes (medidos con `herramientas/consumo.ps1` el 2026-11-24, con margen):
  - .exe ≤ 128 KB (íconos incluidos; `build.ps1` avisa).
  - Memoria privada ≤ 2,5 MB (hoy 1,5–1,8 MB). Excepción: `cube` ≤ 7 MB (tablas del
    solver, ~3,7 MB; hoy 5,3 MB).
  - CPU en reposo: ≤ 5 ms en 3 s (hoy 0).
  - Dibujo: ≤ 3 ms por frame en promedio (hoy 0,3–1,5 ms).
  Quien supere un tope lo explica en su ficha (Excepciones) y en `consumo.ps1`.

### Persistencia (.ini)

- Se guardan ajustes y récords o estadísticas. Una medición a medias, nunca; un juego
  a medias, solo según «Cerrar» (arriba).
- Se guarda el valor real (`time=30`), nunca la posición en una lista.
- Se lee siempre validando: `cfg_get_range(clave, def, min, max)` y
  `cfg_get_choice(clave, def, valores, n)`; un valor no permitido usa el por defecto.
- Claves en inglés, minúsculas, sin tildes, `_` entre palabras (`inspection`,
  `best`, `turn_ms`). La sección es el `name` del widget.
- Si una clave cambia de significado, se usa otro nombre y la vieja se ignora; nunca
  se reutiliza un nombre.
- Se escribe al elegir un ajuste o al terminar una partida; nunca en `on_tick`, con
  autorrepetición ni durante un arrastre (lo continuo se guarda al soltar).

### Menú propio

- `menu` / `menu_checked` / `on_menu`. `L">Título"` abre un submenú, `L"<"` lo cierra,
  `L"-"` es un separador.
- Índices con enumeración e inicializadores designados (`[M_INSPECTION] = L"…"`);
  nunca números sueltos en `menu_checked` / `on_menu`.
- Orden: modos y opciones frecuentes → opciones que se activan y desactivan →
  submenús → separador → acciones destructivas al final. Un grupo excluyente de más
  de 3 opciones va en submenú. Si una opción tiene atajo, se muestra con `\t`.
- Marcas: `menu_checked` devuelve 1 para ✓ (se activa y desactiva) y 2 para el punto
  • (una de varias).
- Acciones destructivas con submenú de confirmación («Borrar tiempos y récord ▸ Sí,
  borrar todo»); el nombre dice exactamente qué se borra. Nada de ventanas de
  confirmación.
- Todo widget con récords o estadísticas ofrece borrarlos, con ese formato.

### Récords y resultados

- Toda actividad que termina muestra su resultado como estado persistente hasta que
  se empieza otra vez.
- Mostrar el récord es opcional (lo decide la ficha); avisar de uno nuevo es
  obligatorio: el valor en `accent` mientras dura el resultado y, si hay lugar,
  «¡nuevo récord!» en `accent`.
- Sin récord todavía: «—» donde iría el valor, nunca 0.
- Un resultado inválido (DNF, precisión bajo el mínimo…) no cuenta y se dice por qué
  en `error`.
- Un récord por cada combinación de ajustes que cambie la dificultad; la ficha lista
  cuáles.

### Ayuda

- Aviso para empezar: en reposo, el único gesto para empezar como texto secundario
  («espacio: mezclar», «clic para escribir», el botón de jugar), solo mientras no haya
  un resultado que mostrar.
- «Controles ▸»: submenú que la base genera de la lista `.help` del `WidgetDesc`
  (líneas grises, «Espacio · mezclar»). Es la fuente única de los controles.
- El aviso de foco («clic para …») no es ayuda: se queda aunque haya resultado.

### Sonido

Sin sonido por defecto. Un widget puede ofrecerlo solo para avisos que importan cuando
no estás mirando (8 y 12 s de la inspección, un temporizador que termina), nunca como
adorno. Opción desactivada por defecto, en el menú; suena con `widget_beep` (tonos
cortos generados, sin archivos de audio; se programa en la base cuando un widget lo
necesite).

### Idiomas y textos

- Interfaz en español e inglés, preparada para más. Cada texto visible tiene un
  identificador (`T_BEST`) en la tabla del widget (una por idioma, inicializadores
  designados) y se dibuja con `tr(T_BEST)`. Si a un idioma le falta un texto, usa el
  español. La base hace lo mismo con sus textos.
- Idioma: el de Windows por defecto; submenú común «Idioma ▸ Automático / Español /
  English», guardado en el .ini.
- Ningún texto visible fuera de la tabla. Frases con datos enteras, con su hueco
  (`L"Mejor: %s"`), nunca trozos pegados. Evitar plurales que dependen del número (o
  dos textos). Todo idioma debe caber (las fotos de `llenado.ps1` cubren cada idioma).
- El contenido (p.ej. listas de palabras) no es interfaz: tiene su propio selector.
- Estilo: tuteo, español neutro sin regionalismos; avisos en minúscula y cortos;
  opciones del menú con mayúscula inicial; unidades con espacio («15 s»); números con
  la convención de la actividad (punto decimal en tiempos del cubo), si no, coma.

### Nombres y código

- Principio: lo que es código (identificadores, claves) va en inglés; lo que ve el
  usuario (textos, menús), en su idioma.
- Widget: nombre propio en inglés que no se traduce. Una palabra, minúsculas, sin
  tildes, único y que no choque con un comando de Windows (p.ej. `type` no). Es
  carpeta, .exe, `name`, sección del .ini y mutex. Su título traducible es `T_TITLE`.
- Código: funciones y variables en minúsculas con `_`, lo del widget `static`;
  constantes y enumeraciones en MAYÚSCULAS con prefijo por grupo (`M_` menú, `T_`
  textos, `MSG_` mensajes). `widget_`, `gfx_` y `cfg_` son solo de la base.
- Comentarios en español con tildes. `.c`, `.h`, `.md`, `.txt` en UTF-8 sin BOM; `.ps1`
  en ASCII (PowerShell 5.1 lee como ANSI los archivos sin BOM).

### Origen y licencias

- Se pueden tomar mecánicas, reglas, mapas de teclas y convenciones del dominio; nunca
  código, textos, gráficos, nombres ni el aspecto de otro programa. Lo tomado va en la
  ficha (Origen) y con crédito en el README.
- Datos de terceros solo con licencia que permita redistribuir, con un LEEME junto a
  los datos (origen, licencia, cómo se generaron).
- Licencia del proyecto: MIT. Las listas de palabras: CC BY-SA 4.0.

## Tamaño y espacio (obligatorio)

Coordenadas en píxeles lógicos (la base escala según el DPI).

**Conceptos**
- Celda: el tamaño de la ventana. Ancho y alto SIEMPRE múltiplos de 40, de 80 a 320.
- Margen: `MARGIN` (6 px) mínimo entre el borde y el contenido visible.
- Estados: cada pantalla distinta del widget.
  - **Transitorio** si cumple las tres cosas: no trae información que haya que leer,
    termina solo sin que el usuario haga nada, y dura ≤ 300 ms con los valores por
    defecto (si el usuario elige algo más lento en el menú, se acepta). P.ej. un giro
    del cubo o un destello. Puede usar el margen, nunca salirse de la ventana, y no
    cuenta para el llenado.
  - **Persistente:** todo lo demás, aunque dure poco (p.ej. la cuenta de la
    inspección o el aviso «falló»). Respeta el margen.
- La base pega los widgets con imán, SIN separación, a los bordes de la pantalla y entre
  sí (atrae a menos de 12 px; Shift al arrastrar lo desactiva). El margen es lo que
  separa a dos widgets pegados.

**Papel de cada elemento** (mínimo y tope, pensados para opacidad baja)

| Papel | Ejemplos | Mínimo | Tope |
|---|---|---|---|
| Texto secundario | encabezados, avisos, unidades | 11 px | 13 px |
| Texto principal | texto a leer o escribir, marcador, datos | 14 px | 16 px |
| Dato destacado | tiempo del cubo, WPM del resultado | 24 px | sin tope |
| Gráfico | cubo, teclas, área de juego | detalles ~10 px | sin tope |
| Zona para clicar | botones, el punto | 16×16 px | — |

Interlineado del texto de varias líneas: entre 1,5× (mínimo de WCAG) y 1,7× el tamaño
de la letra.

**Elegir la celda**
1. Contenido mínimo = requisitos funcionales con cada elemento en el mínimo de su papel.
2. De las celdas donde ese contenido cabe con su margen, la de MENOR ÁREA.
3. Empate: la de forma más parecida al contenido. Nunca más de 320×320 (si no cabe, se
   recortan los requisitos).

**Llenar la celda**
1. Agrandar los elementos dentro de su rango (primero gráficos y datos destacados) hasta
   que, juntando todos los estados persistentes (en todos los idiomas), el contenido
   llegue a 6–10 px del borde en al menos un eje. Si cambia de forma (cámara del cubo),
   manda el caso más grande.
2. Lo que sobre en el otro eje: centrar; y/o abrir el interlineado hasta 1,7×.
3. Si con todo en su tope sobran 40 px o más en un eje (sumando ambos lados, sin contar
   los márgenes), la celda es demasiado grande: volver a «Elegir la celda».

**Excepciones** (cualquier otra desviación es un error; cada una va también en la
ficha del widget)
- Mapa de teclas: etiquetas de 8 y 10 px; las 40 teclas deben entrar en 240 px.
- `dot`: el punto se achica hasta 8 px de diámetro; es la mecánica del juego.

**Hoy:** dot 160×120, cube 160×200, keys 240×120, typing 280×120.

## Ficha (obligatoria)

Comentario al inicio del .c principal de cada widget, con estos títulos en este orden.
Todos son obligatorios; «—» si no aplica (así se distingue de un olvido).

```c
/*
 * <nombre> - <qué es, en una línea>.
 *
 * Ficha (ver CLAUDE.md):
 *   Tipo: medición | juego | otro
 *   Requisitos: <qué debe mostrar para usarse bien, p.ej. «3 líneas de 30 caracteres»>
 *   Estados persistentes: <…>   Transitorios: <…>
 *   Papeles: <elemento = papel (tamaño px)>, …
 *   Celda: <W>x<H>. <por qué: mínimo, qué no cabe en la menor, cómo se llena>
 *   Al ocultar / sin foco / al cerrar: <qué pasa>
 *   Aviso para empezar: <texto o gesto>
 *   Controles: ver HELP
 *   Récords: <cuáles; qué ajustes los separan>
 *   Claves del .ini: <clave = qué guarda (rango)>, …
 *   Mensajes: envía <MSG_…> a <widget>; recibe <…>
 *   Colores del juego: <constante = qué representa>
 *   Sonido: <qué aviso y cuándo>
 *   Origen: <mecánica tomada de…; licencia de los datos>
 *   Excepciones: <regla que no cumple y por qué>
 */
```

## Verificación

Nunca abrir ventanas de prueba sobre la pantalla.

- **Modo foto:** `FIDGETS_SNAPSHOT=archivo` dibuja sin ventana, guarda el buffer y sale.
  `FIDGETS_EVENTS` es un guion de eventos en orden, p.ej.
  `"click:80,68 tick:2000 hide show key:17 text:hola blur lang:en"` (`text` admite
  `\b`, `\t`, `\n` para retroceso, Tab y Enter). `FIDGETS_SEED` fija el azar.
- **Llenado:** `.\herramientas\llenado.ps1 [widget] [-Detalle]`: margen en cada estado,
  llenado por grupo de estados y sobrante de 40 px, en cada idioma. Al agregar un
  widget o un estado, agregar sus casos al script.
- **Consumo:** `.\herramientas\consumo.ps1 [widget]` abre el widget fuera de todos los
  monitores (`FIDGETS_MEASURE`, sin barra de tareas) y mide memoria privada y CPU en
  reposo y el tiempo por frame animando.
- A mano, contra la ficha: tamaños de letra, requisitos funcionales y cuáles elementos
  son decorativos.

## Publicación

Etapas: 1) widgets independientes + ajustes en `%APPDATA%` (hecho); 2) publicar en
GitHub (hecho: repo público, releases con .zip); 3) si crece, un centro de widgets con
catálogo.

- winget, pospuesto: por ahora solo se ofrecen los .zip del release. Un portable de
  winget no aparece en el menú Inicio, lo que estorba a widgets con ventana. Si se
  retoma: un paquete por widget (`frankArgomedo.fidgets.<nombre>`) con alias
  `fidgets-<nombre>` para no ocupar nombres genéricos en el PATH.

- Una sola versión para todo el proyecto (SemVer desde `0.1.0`, tags `vX.Y.Z`), en el
  archivo `VERSION`. `build.ps1` le pone a cada .exe su `VERSIONINFO` (versión,
  `FileDescription` = nombre del widget, producto `fidgets`, autor `frankArgomedo`) y
  su ícono `widgets/<nombre>/icon.ico`.
- Íconos: los genera `herramientas/iconos.py` (16, 32, 48 y 256 px PNG de 32 colores,
  ~4 KB; cuentan para el tope de 128 KB). Estilo común: cuadrado redondeado oscuro con
  un dibujo simple en colores de la paleta (o del juego). Al agregar un widget, se le
  agrega su dibujo al script.
- `CHANGELOG.md` en inglés (Keep a Changelog), indicando qué widgets cambiaron.
- `README.md` en inglés y `README.es.md` en español, enlazados; al agregar un widget se
  actualizan los dos en el mismo commit (tabla de widgets y su sección).
- Commits en inglés.
- GitHub Releases: `.\herramientas\release.ps1` arma en `dist\<versión>\` un .zip por
  widget y uno con todos (con LICENSE, los README y la licencia de las listas de
  palabras si va `typing`) y `SHA256SUMS.txt`. Exige un árbol limpio y la sección de la
  versión en el CHANGELOG. Sin firma de código por ahora: los .exe muestran el aviso de
  SmartScreen (explicado en el README).
- Para publicar una versión: actualizar `VERSION` y `CHANGELOG.md`, commit, tag
  `vX.Y.Z`, `release.ps1`, y subir lo de `dist\<versión>\` a un release del tag
  (con la sección del CHANGELOG como texto).
