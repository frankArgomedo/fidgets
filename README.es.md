# fidgets

[English](README.md)

Widgets diminutos para Windows para pasar el rato: minijuegos y otras cosas que se
quedan en pantalla de forma discreta. Sin bordes, con transparencia, nunca roban el
foco y casi no consumen recursos (0 % de CPU en reposo, ~2 MB de memoria). Cada
widget es un `.exe` independiente de unos 100 KB, escrito en C11 + Win32 + GDI, sin
dependencias.

## Widgets

| Widget | Qué es |
| --- | --- |
| `cube` | Timer de speedcubing con un cubo virtual 3×3 que giras con el teclado: mezclas random-state, inspección de 15 s, giros animados y mapa de teclas opcional. |
| `typing` | Test de mecanografía: palabras al azar contra el reloj (15–120 s) o por cantidad (10–100), en español o inglés, con puntuación, números y dificultades. Muestra WPM, precisión y tu récord. |
| `dot` | Juego de reflejos: haz clic en el punto que se encoge antes de que se esfume. Tres vidas. |

## Instalar

1. Descarga el `.zip` del widget que quieras (o `fidgets-<versión>.zip` con todos)
   desde [Releases](../../releases).
2. Descomprímelo donde quieras y abre el `.exe`. No hay instalador ni nada más que
   instalar.

Los `.exe` no están firmados, así que la primera vez Windows SmartScreen puede
mostrar un aviso: haz clic en **Más información → Ejecutar de todas formas**. Cada
versión publica el SHA-256 de cada archivo para que compruebes lo que descargaste, o
puedes [compilarlo tú](#compilar).

## Cómo se usan

Todos los widgets funcionan igual:

| Acción | Cómo |
| --- | --- |
| Mover | Arrastrar con el botón izquierdo (Ctrl + arrastrar siempre funciona). Se pega a los bordes de la pantalla y a otros widgets; con Shift no se pega |
| Menú (encima de todo, atenuar, opacidad, idioma, ocultar, cerrar) | Clic derecho |
| Controles de este widget | Clic derecho → Controles |
| Cerrar este widget | Clic central, o «Cerrar» en el menú |
| Ocultar / mostrar todos | Ctrl+Alt+B |
| Cerrar todos | Ctrl+Alt+Q |

- Un widget solo recibe el teclado cuando le haces clic. Si lo necesita y no lo
  tiene, lo avisa.
- Esc cancela lo que está en curso; nunca cierra un widget.
- Ocultar un widget, bloquear la PC o suspenderla nunca te cuesta una partida o un
  tiempo: las mediciones se cancelan y los juegos se pausan.
- La interfaz sigue el idioma de Windows (español o inglés); se cambia en «Idioma» en
  el menú.
- Los ajustes y récords se guardan en `%APPDATA%\fidgets\<widget>.ini`.

### cube

- Espacio mezcla (y empieza la inspección de 15 s, si está activa). El primer giro
  arranca el tiempo; resolver el cubo lo detiene.
- Las letras giran el cubo con el mapa de teclas de csTimer. Las teclas se leen por su
  posición física, así que el mapa es el mismo con cualquier distribución de teclado.
  Activa **Mapa de teclas** en el menú para verlo junto al cubo (`cube.exe --keys`).
- Las flechas giran la cámara. Esc aborta (con el tiempo corriendo cuenta como DNF).
- Menú: inspección, velocidad de giro, mapa de teclas, borrar tiempos.

### typing

- Haz clic en el widget y empieza a escribir; el tiempo arranca con la primera letra.
- Espacio pasa a la siguiente palabra; Retroceso corrige; Ctrl+Retroceso borra la
  palabra.
- Tab te da palabras nuevas; Esc cancela el test.
- Menú: modo (tiempo o palabras), duración, idioma y lista de palabras, puntuación,
  números, sin tildes, dificultad (normal; experto: fallas si dejas una palabra mal;
  maestro: fallas con el primer error).
- Un resultado con precisión menor al 75 % no cuenta como récord.

### dot

- Haz clic en el botón para jugar. Cada punto que aciertas suma; fallarlo o dejar que
  se esfume te quita una vida.

## Compilar

Requiere [Zig](https://ziglang.org/) (se usa como compilador de C):

```powershell
winget install zig.zig
.\build.ps1          # compila todos los widgets en bin\
.\build.ps1 cube     # compila solo uno
```

Herramientas de desarrollo (en `herramientas\`, no hacen falta para compilar):

- `llenado.ps1`: dibuja cada estado de cada widget fuera de pantalla y revisa márgenes
  y llenado en cada idioma.
- `consumo.ps1`: mide la memoria, la CPU en reposo y el tiempo de dibujo de cada widget
  fuera de pantalla.
- `release.ps1`: arma los `.zip` de una versión y su SHA-256.
- `iconos.py`: vuelve a generar los íconos (necesita Python con Pillow).

Las reglas del proyecto están en [CLAUDE.md](CLAUDE.md).

## Créditos

- `cube`: el mapa de teclas, el flujo del timer, la cámara y los colores siguen las
  convenciones de [csTimer](https://github.com/cs0x7f/cstimer); las mezclas usan una
  implementación propia del algoritmo de dos fases de Herbert Kociemba. No se tomó
  código.
- `typing`: la mecánica del test y las fórmulas de WPM y precisión siguen a
  [monkeytype](https://github.com/monkeytypegame/monkeytype). No se tomó código.
- Listas de palabras derivadas de [wordfreq](https://github.com/rspeer/wordfreq)
  (CC BY-SA 4.0); ver [widgets/typing/words/LEEME.md](widgets/typing/words/LEEME.md).

## Licencia

[MIT](LICENSE) © frankArgomedo. Las listas de palabras de `widgets/typing/words/` son
CC BY-SA 4.0.
