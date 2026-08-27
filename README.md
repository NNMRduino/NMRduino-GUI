# NMRduino GUI

Desktop control and acquisition software for the [NMRduino](https://github.com/NNMRduino/NMRduino) hardware.

**Compatibility:** requires NMRduino firmware v1.2.0+.

A pre-built Windows release is available on the [latest release](https://github.com/NNMRduino/NMRduino-GUI/releases/latest) page.

## Included tasks

- Spectrum Analyzer
- Pulse and Acquire
- Task Scheduler

## Build

Requirements: Qt 5.15.2, a C++14-capable compiler.

1. Open `src/NMRduino.pro` in Qt Creator.
2. Select a MinGW 64-bit kit (the project statically links the MinGW runtime on
   Windows; an MSVC kit will also build, just without that optimization).
3. Build in Release mode.

## Run

- **Portable mode**: place a `_config.ini` file next to `nmrduino.exe` and all
  settings/logs are kept alongside the executable.
- **Default mode**: if no `_config.ini` is found next to the executable, settings and
  logs are stored in `%USERPROFILE%\.NMRduino\` (Windows) / `~/.NMRduino/` (Linux).

## Windows release

Windows builds are distributed as a self-contained zip (no installer). Build and
package it in one step with:

```powershell
tools\windows_deploy.ps1 -QtDir <path to Qt kit, e.g. E:\Qt\5.15.2\mingw81_64> -MingwDir <path to matching MinGW toolchain, e.g. E:\Qt\Tools\mingw810_64>
```

This runs `qmake`/`mingw32-make` for a Release build, then packages `nmrduino.exe`
with its runtime dependencies into `dist\nmrduino-<version>-win64\` and a matching
zip. It does **not** use `windeployqt` — that tool has a known bad debug/release
detection heuristic on the Qt 5.15.2 MinGW open-source package that makes it fail
with "Unable to find the platform plugin" and deploy nothing. Instead the script
walks the real PE import table (via `objdump`) of the built exe and its Qt/MinGW
DLLs, recursively, and adds the platform/imageformat/style plugins the app actually
needs. It finishes with a smoke test that launches the packaged exe standalone and
confirms it stays running.

## License

Licensed under GPL-3.0 — see [`LICENCE`](LICENCE).
