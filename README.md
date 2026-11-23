# widgets

Widgets diminutos para Windows para pasar el rato: se quedan en pantalla de forma
discreta (sin bordes, con transparencia, opcionalmente encima de todo) y casi no
consumen recursos. Cada widget es un `.exe` independiente de menos de 100 KB, escrito
en C11 + Win32 + GDI, sin dependencias.

## Widgets

| Widget | Qué es |
| --- | --- |
| `cubo` | Timer de speedcubing con cubo virtual 3×3 controlado por teclado, al estilo csTimer: mezclas random-state (solver de dos fases de Kociemba), inspección de 15 s, animación de giros y mapa de teclas opcional al lado. |
| `punto` | Minijuego de reflejos: haz clic en el punto. |

## Controles comunes

| Acción | Atajo |
| --- | --- |
| Mover | Arrastrar con clic izquierdo (Ctrl + arrastrar siempre funciona) |
| Menú (encima de todo, atenuar, opacidad, ocultar, cerrar) | Clic derecho |
| Opacidad | Rueda del mouse |
| Cerrar este widget | Esc o clic central |
| Ocultar / mostrar todos | Ctrl+Alt+B |
| Cerrar todos | Ctrl+Alt+Q |

### cubo

- Las teclas siguen el mapa de csTimer y se leen por posición física, así que
  funcionan igual con cualquier distribución de teclado (español, inglés, etc.).
- Espacio: mezclar. Flechas: girar la cámara. Esc: abortar la resolución.
- Menú propio: inspección, mapa de teclas, velocidad de giro, borrar tiempos.

## Compilar

Requiere [Zig](https://ziglang.org/) (se usa `zig cc` como compilador de C):

```powershell
winget install zig.zig
.\build.ps1          # compila todos los widgets en bin\
.\build.ps1 cubo     # compila solo uno
```

## Ajustes

Cada widget guarda su posición, opacidad y opciones en
`%APPDATA%\bored\<widget>.ini`.

## Nota sobre SmartScreen

Los `.exe` no están firmados, así que al descargarlos Windows puede mostrar el aviso
de SmartScreen («Más información» → «Ejecutar de todas formas»). Compilar desde el
código evita el aviso.
