# Changelog

All notable changes to this project are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/). One version covers every widget; each
entry says which widgets changed.

## [Unreleased]

### Changed

- `typing`: the `.exe` is 16 KB smaller (125 → 109.5 KB); the word-list index no
  longer stores empty space in the file.

## [0.1.0] - 2026-11-24

First public version.

### Added

- `cube`: speedcubing timer with a keyboard-driven virtual 3×3 cube, random-state
  scrambles (two-phase solver), 15 s inspection, adjustable turn speed, camera
  control, DNF, best time, and an optional key map (`cube.exe --keys`).
- `typing`: typing test by time (15–120 s) or by word count (10–100), English and
  Spanish word lists (top 200 / top 1000), punctuation, numbers, no-accents mode,
  three difficulties, WPM, raw WPM, accuracy and a record per combination of
  settings.
- `dot`: reflex game with three lives, pause and best score.
- Shared by every widget: borderless see-through window, drag and snap, right-click
  menu (always on top, dim, opacity, language, controls, hide, close), Ctrl+Alt+B to
  hide/show all, Ctrl+Alt+Q to close all, one instance per widget, English and
  Spanish interface, settings in `%APPDATA%\fidgets`, pause or cancel without penalty
  when hidden, locked or suspended, and the Windows "show animations" setting
  respected.
- Icons and version information in every `.exe`.

[Unreleased]: ../../compare/v0.1.0...HEAD
[0.1.0]: ../../releases/tag/v0.1.0
