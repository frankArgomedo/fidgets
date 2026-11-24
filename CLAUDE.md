# bored

Colección de widgets para pasar el rato (minijuegos y otras cosas) que se quedan
en pantalla de forma discreta: pequeños, sin bordes, con transparencia y con el
menor consumo posible. Solo para Windows.

## Stack

- **C11 + Win32 + GDI**, sin dependencias externas. Se compila con `zig cc`
  (instalado con winget, `zig.zig`). Nada de runtimes (.NET, Electron, Python, etc.).
- `.\build.ps1` compila todo; `.\build.ps1 <nombre>` compila uno. Salida en `bin/`.
- Configuración: `%APPDATA%\bored\<exe>.ini`, una sección por widget (`BORED_INI=ruta` la
  reemplaza en pruebas). Al primer arranque se migra el `bin\bored.ini` viejo si existe.

## Estructura

- `common/widget.h|.c`: la base común (ventana layered, menú, atajos, .ini, dibujo).
  Toda la funcionalidad compartida va aquí, no se copia en cada widget.
- `widgets/<nombre>/*.c`: un widget por carpeta; el nombre de la carpeta es el del .exe.
  Cada widget define un `WidgetDesc` y llama a `widget_run`.
- Cada widget es un programa independiente (se instala y comparte por separado).
  Un "acompañante" que solo tiene sentido con otro widget vive dentro de su .exe y se
  abre con un argumento: el mapa de teclas es `cubo.exe --teclas` (`widgets/cubo/teclas.c`),
  lo abre/cierra el cubo desde su menú y se cierra con él.
- Plan de distribución por etapas: 1) widgets independientes + ajustes en %APPDATA%
  (hecho); 2) publicar (GitHub, quizá winget) cuando haya varios; 3) si crece, un
  centro de widgets con catálogo. Los .exe sin firma muestran el aviso de SmartScreen.

## Reglas para widgets nuevos

- Tamaño, margen y llenado: ver «Tamaño y espacio (obligatorio)».
- Sin panel de fondo por defecto (preferencia del usuario): `gfx_clear_hit()` deja el
  fondo invisible pero agarrable con cualquier opacidad. Los píxeles con alfa final 0
  dejan pasar el clic a lo que está detrás. Texto con `| TEXT_SHADOW` para que se
  lea sobre fondos claros.
- La opacidad (menú o rueda del mouse, de 5 en 5) se aplica a todo el widget; el
  usuario la tiene casi siempre baja (~25%) y a veces la sube.
- Pruebas visuales sin ventanas: `BORED_SNAPSHOT=archivo` (+ `BORED_KEYS=scan,scan…`,
  `BORED_TEXT=texto` para `on_char`, con `\b`, `\t` y `\n` como retroceso, Tab y Enter, `BORED_TICK_MS=n` para simular n ms de animación) dibuja una vez en un archivo y sale. Nunca abrir ventanas de prueba sobre la pantalla.
- Consumo: `fps = 0` siempre que no haya animación. Usar `widget_set_fps(30)`
  solo mientras algo se mueve, volver a 0 al terminar, y llamar a `widget_redraw()`
  cuando cambie el estado. Evitar asignar memoria por frame.
- `on_mouse` devuelve 0 en zonas "vacías" para que se pueda arrastrar la ventana.
- No volver a implementar lo que ya da la base: arrastrar, el menú del clic derecho
  (encima de todo, atenuar, opacidad, ocultar, cerrar), clic central para cerrar,
  Ctrl+Alt+B para ocultar o mostrar todos y Ctrl+Alt+Q para cerrar todos.
- Teclado: `on_key` recibe el scan code físico; úsalo para mapas de teclas
  (independiente del idioma del teclado). Esc NUNCA cierra un widget (preferencia del
  usuario: se pulsa sin querer); cada widget puede usarlo, p.ej. para cancelar.
  Para escribir texto usa `on_char` (carácter ya compuesto: tildes, ñ, mayúsculas).
- Opciones propias del widget: `menu` / `menu_checked` / `on_menu`; `L">Título"` abre un
  submenú, `L"<"` lo cierra y `L"-"` es un separador (los índices son posiciones del arreglo).
- Colores: siempre de la paleta común `theme->text|sub|accent|error|extra` (base para
  temas futuros), nunca colores sueltos para texto o avisos. `TEXT_MONO` = Consolas.
- Datos grandes (listas, textos) en archivos junto al widget, incluidos con `#embed`.
- Comunicación entre widgets: `widget_post(L"nombre", code, value)` (no bloquea; no hace
  nada si el otro no está abierto) y `on_message` en el receptor. Ej.: `cubo` avisa a
  `teclas` cada tecla usada. El mapa de teclas compartido está en `widgets/cubo/keymap.h`.
- Referencia en reposo (`punto`): CPU 0 %, ~1,3 MB de memoria privada, .exe de ~70 KB.

## Tamaño y espacio (obligatorio)

Coordenadas en píxeles lógicos (la base escala según el DPI).

**Conceptos**
- Celda: el tamaño de la ventana. Ancho y alto SIEMPRE múltiplos de 40, de 80 a 320.
- Margen: `MARGIN` (6 px) mínimo entre el borde y el contenido visible.
- Estados: cada pantalla distinta del widget. Persistentes (se quedan) respetan el
  margen; transitorios (< ~100 ms, p.ej. un giro del cubo) pueden usar el margen pero
  no salirse de la ventana.
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

**Ficha** (obligatoria, en el comentario inicial de cada widget): requisitos funcionales
(qué debe mostrar para usarse bien, p.ej. «3 líneas de al menos 30 caracteres»), estados
persistentes y transitorios, papel de cada elemento, y celda elegida con su porqué.

**Elegir la celda**
1. Contenido mínimo = requisitos funcionales con cada elemento en el mínimo de su papel.
2. De las celdas donde ese contenido cabe con su margen, la de MENOR ÁREA.
3. Empate: la de forma más parecida al contenido. Nunca más de 320×320 (si no cabe, se
   recortan los requisitos).

**Llenar la celda**
1. Agrandar los elementos dentro de su rango (primero gráficos y datos destacados) hasta
   que, juntando todos los estados persistentes, el contenido llegue a 6–10 px del borde
   en al menos un eje. Si cambia de forma (cámara del cubo), manda el caso más grande.
2. Lo que sobre en el otro eje: centrar; y/o abrir el interlineado hasta 1,7×.
3. Si con todo en su tope sobran 40 px o más en un eje (sumando ambos lados, sin contar
   los márgenes), la celda es demasiado grande: volver a «Elegir la celda».

**Excepciones** (cualquier otra desviación es un error)
- Mapa de teclas: etiquetas de 8 y 10 px; las 40 teclas deben entrar en 240 px.
- `punto`: el punto se achica hasta 8 px de diámetro; es la mecánica del juego.

**Verificación**
- `.\herramientas\llenado.ps1 [widget] [-Detalle]` (fotos sin ventanas): margen en cada
  estado, llenado por grupo de estados y sobrante de 40 px. Al agregar un widget o un
  estado, agregar sus casos al script.
- Tamaños de letra y requisitos funcionales: revisar a mano contra la ficha.

**Hoy:** punto 160×120, cubo 160×200, mapa 240×120, teclear 280×120.
