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

- Tamaño: cuadrícula de 40 px. Ancho y alto SIEMPRE múltiplos de 40 (160, 240, 320…;
  80, 120, 160, 200…), con la forma que pida el contenido y lo más chico posible
  (máximo ~320×320). Hoy: punto 160×120, cubo 160×200, mapa 240×120, teclear 320×120.
  La base pega los widgets con imán, SIN separación, a los bordes de la pantalla y
  entre sí (atrae a menos de 12 px; Shift al arrastrar lo desactiva); con medidas
  múltiplos de 40 los bordes coinciden.
- Margen OBLIGATORIO: el contenido visible queda al menos `MARGIN` (6 px) dentro del
  borde del widget, en todos sus estados; ese margen es lo que separa a dos widgets
  pegados. Los estados de menos de ~100 ms (p.ej. un giro del cubo) pueden usar el
  margen, pero no salirse de la ventana.
- Llenar la celda: el margen es el mínimo, no un relleno. El contenido se dibuja lo más
  grande posible y, juntando todos sus estados, llega a ~6–10 px del borde en al menos
  un eje (el otro queda centrado). Si cambia de forma (cámara del cubo), se dimensiona
  por el caso más grande.
- Cómo elegir el tamaño al crear un widget: 1) contenido mínimo legible; 2) la celda más
  chica de la cuadrícula donde entra con su margen; 3) agrandar el contenido hasta
  llenarla. Es demasiado chico si, llenando la celda, queda bajo algún mínimo; es
  demasiado grande si cabría en una celda menor respetando los mínimos.
- Mínimos de legibilidad (pensados para opacidad baja): texto principal 14 px, texto
  secundario 11 px, zona para clicar 16×16 px, detalles de color ~10 px por lado.
  Excepciones conocidas: etiquetas del mapa de teclas (8 y 10 px) y el punto que se
  achica en `punto` (es la mecánica del juego).
- Comprobar margen y llenado con `.\herramientas\llenado.ps1 [widget] [-Detalle]` (fotos
  sin ventanas); al agregar un widget o un estado, agregar sus casos al script.
  Coordenadas en píxeles lógicos (la base escala según el DPI).
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
