# fidgets

Widgets diminutos para Windows para pasar el rato: se quedan en pantalla de forma
discreta (sin bordes, con transparencia, opcionalmente encima de todo) y casi no
consumen recursos. Cada widget es un `.exe` independiente de ~100 KB, escrito
en C11 + Win32 + GDI, sin dependencias.

## Widgets

| Widget | Qué es |
| --- | --- |
| `cube` | Timer de speedcubing con cubo virtual 3×3 controlado por teclado, al estilo csTimer: mezclas random-state (solver de dos fases de Kociemba), inspección de 15 s, animación de giros y mapa de teclas opcional al lado. |
| `typing` | Test de mecanografía: palabras al azar contra el reloj (15–120 s) o por cantidad (10–100), en español o inglés, con puntuación, números, modo sin tildes y dificultades. Muestra WPM, precisión y récord. |
| `dot` | Minijuego de reflejos: haz clic en el punto. |

## Controles comunes

| Acción | Atajo |
| --- | --- |
| Mover | Arrastrar con clic izquierdo (Ctrl + arrastrar siempre funciona). Se pega a los bordes y a otros widgets; con Shift no se pega |
| Menú (encima de todo, atenuar, opacidad, ocultar, cerrar) | Clic derecho |
| Opacidad | Rueda del mouse |
| Cerrar este widget | Clic central (o «Cerrar» en el menú) |
| Ocultar / mostrar todos | Ctrl+Alt+B |
| Cerrar todos | Ctrl+Alt+Q |

### cube

- Las teclas siguen el mapa de csTimer y se leen por posición física, así que
  funcionan igual con cualquier distribución de teclado (español, inglés, etc.).
- Espacio: mezclar. Flechas: girar la cámara. Esc: abortar la resolución.
- Menú propio: inspección, mapa de teclas, velocidad de giro, borrar tiempos.

### typing

- Haz clic en el widget y empieza a escribir: el tiempo arranca con la primera letra.
- Espacio pasa a la siguiente palabra; Retroceso vuelve a la anterior solo si tenía un error.
- Tab reinicia con palabras nuevas; Esc cancela el test en curso. Modo, idioma y opciones en el clic derecho.
- Las listas de palabras están en `widgets/typing/words/` (CC BY-SA 4.0, derivadas de wordfreq).

## Compilar

Requiere [Zig](https://ziglang.org/) (se usa `zig cc` como compilador de C):

```powershell
winget install zig.zig
.\build.ps1          # compila todos los widgets en bin\
.\build.ps1 cube     # compila solo uno
```

## Ajustes

Cada widget guarda su posición, opacidad y opciones en
`%APPDATA%\fidgets\<widget>.ini`.

## Nota sobre SmartScreen

Los `.exe` no están firmados, así que al descargarlos Windows puede mostrar el aviso
de SmartScreen («Más información» → «Ejecutar de todas formas»). Compilar desde el
código evita el aviso.
