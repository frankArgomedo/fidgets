# fidgets

[Español](README.es.md)

Tiny Windows widgets to fidget with: small games and gadgets that sit quietly on your
screen. No borders, see-through, never steal focus, and they use almost no resources
(0% CPU when idle, ~2 MB of memory). Each widget is a standalone `.exe` of about
100 KB, written in C11 + Win32 + GDI with no dependencies.

## Widgets

| Widget | What it is |
| --- | --- |
| `cube` | Speedcubing timer with a virtual 3×3 cube you turn with the keyboard: random-state scrambles, 15 s inspection, animated turns and an optional key map. |
| `typing` | Typing test: random words against the clock (15–120 s) or by count (10–100), in English or Spanish, with punctuation, numbers and difficulty options. Shows WPM, accuracy and your best. |
| `dot` | Reflex game: click the shrinking dot before it fades. Three lives. |

## Install

1. Download the `.zip` of the widget you want (or `fidgets-<version>.zip` with all of
   them) from [Releases](../../releases).
2. Unzip it anywhere and run the `.exe`. There is no installer and nothing else to
   install.

The `.exe` files are not code-signed, so Windows SmartScreen may warn you the first
time: click **More info → Run anyway**. Each release lists the SHA-256 of every file
so you can check what you downloaded, or you can [build it yourself](#build).

## Using them

Every widget works the same way:

| Action | How |
| --- | --- |
| Move | Drag with the left button (Ctrl + drag always works). Snaps to screen edges and other widgets; hold Shift to avoid snapping |
| Menu (always on top, dim, opacity, language, hide, close) | Right click |
| Controls of this widget | Right click → Controls |
| Close this widget | Middle click, or Close in the menu |
| Hide / show all widgets | Ctrl+Alt+B |
| Close all widgets | Ctrl+Alt+Q |

- A widget only gets the keyboard when you click it. If it needs the keyboard and
  doesn't have it, it says so.
- Esc cancels what is in progress; it never closes a widget.
- Hiding a widget, locking the PC or putting it to sleep never costs you a game or a
  time: measurements are cancelled and games are paused.
- The interface follows the Windows language (English or Spanish); change it in
  Language in the menu.
- Settings and records are saved in `%APPDATA%\fidgets\<widget>.ini`.

### cube

- Space scrambles (and starts the 15 s inspection, if on). The first turn starts the
  timer; solving the cube stops it.
- Letters turn the cube using the csTimer key map. Keys are read by their physical
  position, so the map is the same with any keyboard layout. Turn on **Key map** in
  the menu to see it next to the cube (`cube.exe --keys`).
- Arrows rotate the camera. Esc aborts (while timing, it counts as DNF).
- Menu: inspection, turn speed, key map, clear times.

### typing

- Click the widget and start typing; the timer starts with the first letter.
- Space goes to the next word; Backspace corrects; Ctrl+Backspace deletes the word.
- Tab gives you new words; Esc cancels the test.
- Menu: mode (time or words), duration, word language and list, punctuation, numbers,
  no accents, difficulty (normal; expert: you fail if you leave a word wrong; master:
  you fail on the first mistake).
- A result with accuracy below 75% doesn't count as a record.

### dot

- Click the button to play. Each dot you click is a point; missing it or letting it
  fade costs a life.

## Build

Requires [Zig](https://ziglang.org/) (used as the C compiler):

```powershell
winget install zig.zig
.\build.ps1          # builds every widget into bin\
.\build.ps1 cube     # builds one
```

Development tools (in `herramientas\`, not needed to build):

- `llenado.ps1`: renders every state of every widget off-screen and checks margins and
  layout in each language.
- `consumo.ps1`: measures memory, idle CPU and drawing time of each widget off-screen.
- `release.ps1`: builds the release `.zip` files and their SHA-256.
- `iconos.py`: regenerates the icons (needs Python with Pillow).

The project rules (in Spanish) are in [CLAUDE.md](CLAUDE.md).

## Credits

- `cube`: key map, timer flow, camera and colors follow
  [csTimer](https://github.com/cs0x7f/cstimer) conventions; scrambles use our own
  implementation of Herbert Kociemba's two-phase algorithm. No code was taken.
- `typing`: test mechanics and WPM/accuracy formulas follow
  [monkeytype](https://github.com/monkeytypegame/monkeytype). No code was taken.
- Word lists derived from [wordfreq](https://github.com/rspeer/wordfreq)
  (CC BY-SA 4.0); see [widgets/typing/words/LEEME.md](widgets/typing/words/LEEME.md).

## License

[MIT](LICENSE) © frankArgomedo. The word lists in `widgets/typing/words/` are
CC BY-SA 4.0.
