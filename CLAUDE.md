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

- Tamaño lógico chico: lo normal son ~160×120 y como máximo unos 300×300.
  Coordenadas en píxeles lógicos (la base escala según el DPI).
- Sin panel de fondo por defecto (preferencia del usuario): `gfx_clear_hit()` deja el
  fondo invisible pero agarrable con cualquier opacidad. Los píxeles con alfa final 0
  dejan pasar el clic a lo que está detrás. Texto con `| TEXT_SHADOW` para que se
  lea sobre fondos claros.
- La opacidad (menú o rueda del mouse, de 5 en 5) se aplica a todo el widget; el
  usuario la tiene casi siempre baja (~25%) y a veces la sube.
- Pruebas visuales sin ventanas: `BORED_SNAPSHOT=archivo` (+ `BORED_KEYS=scan,scan…`,
  `BORED_TICK_MS=n` para simular n ms de animación) dibuja una vez en un archivo y sale. Nunca abrir ventanas de prueba sobre la pantalla.
- Consumo: `fps = 0` siempre que no haya animación. Usar `widget_set_fps(30)`
  solo mientras algo se mueve, volver a 0 al terminar, y llamar a `widget_redraw()`
  cuando cambie el estado. Evitar asignar memoria por frame.
- `on_mouse` devuelve 0 en zonas "vacías" para que se pueda arrastrar la ventana.
- No volver a implementar lo que ya da la base: arrastrar, el menú del clic derecho
  (encima de todo, atenuar, opacidad, ocultar, cerrar), Esc o clic central para cerrar,
  Ctrl+Alt+B para ocultar o mostrar todos y Ctrl+Alt+Q para cerrar todos.
- Teclado: `on_key` recibe el scan code físico; úsalo para mapas de teclas
  (independiente del idioma del teclado). Si devuelve 1, Esc ya no cierra el widget.
  Opciones propias del widget: `menu` / `menu_checked` / `on_menu`.
- Comunicación entre widgets: `widget_post(L"nombre", code, value)` (no bloquea; no hace
  nada si el otro no está abierto) y `on_message` en el receptor. Ej.: `cubo` avisa a
  `teclas` cada tecla usada. El mapa de teclas compartido está en `widgets/cubo/keymap.h`.
- Referencia en reposo (`punto`): CPU 0 %, ~1,3 MB de memoria privada, .exe de ~70 KB.
