# Red Storm Rising port: documentation

Red Storm Rising (MicroProse, 1990) for the Amiga, as C++ for Linux with
SDL3. The game's logic was translated routine by routine from the
original 68000 code. It runs on a small model of the Amiga it was
written for, so that it plays exactly as the original, frame for frame.

| Document | What it covers |
|---|---|
| [architecture.md](architecture.md) | The layers (front end, machine, game) and what happens when the game runs: start-up, the programs in turn, frames and time. |
| [game-code.md](game-code.md) | The game code (`src/game`): the five programs, the libraries they share, and a guide to every file. |
| [conventions.md](conventions.md) | How to read the code: references to the original, state and data declarations, names, and why the arithmetic looks the way it does. |
| [testing.md](testing.md) | The gate every commit passes, the scenario scripts, the recordings, and the debugging aids. |
| [reverse-engineering.md](reverse-engineering.md) | The original game: its versions and files, OS calls and hardware, the programs' hand-over, the copy protection, the saves. |
| [known-issues.md](known-issues.md) | Behaviour left as the original had it, with its cause and a possible fix (lost mouse clicks); the accelerated-time fix. |

The history of how the code got here (the translation, then the
restructuring) is in [history/restructure-strategy.md](history/restructure-strategy.md);
what was found out about the original game is in
[reverse-engineering.md](reverse-engineering.md).

## Quick start

```sh
cmake -S . -B build -G Ninja
cmake --build build
build/rsr path/to/RedStormRising_v1.0_1603.lha   # the WHDLoad archive: its files extracted once
build/rsr disk1.adf disk2.adf    # or both disks of v843.02
build/rsr path/to/data           # or the unpacked install's data/ directory
build/rsr                        # after that: the files extracted before (or extracted/ in the source tree)
build/rsr --help                 # the options
```

**The launch screen.** Started without options (`build/rsr`, a
double-click, `run.bat`), the program first shows a launch screen
(`src/launcher.cpp`): the CRT look (Off, Lottes fast, Guest fast, Guest,
Royale), fullscreen or a window, the input (responsive, the default, or
as the original: `--original-input`, see known-issues.md), Start or Quit.

- It works with the mouse or the keys, and remembers the choices in
  `launcher.cfg` in the user's folder.
- If the game files are missing it says so there, with only Quit.
- Any option skips it (the tests always do); `--launcher` and
  `--no-launcher` choose.
- It is drawn with SDL alone: its renderer and its built-in 8x8 font.

The game reads its files straight from the WHDLoad install archive or
from the two disks of v843.02 (ADF images), so they need no unpacking:

- `src/host/game_archive.cpp` has its own LHA decoder (`-lh5-`) and ADF
  reader (AmigaDOS OFS and FFS).
- It checks the 21 files the game uses against the WHDLoad build, and
  refuses another build (such as v843.01). The v843.02 disks hold the same
  build: their cracked `RedStart` gets its two original bytes back.
- It keeps them in the user's folder: `~/.local/share/redstormrising/data`
  (Windows: `%APPDATA%\RedStormRising\data`), where the saves go too.

The game files are not part of the repository (`extracted/` and
`original/` are git-ignored). The port runs the files of the WHDLoad
install (`RedStormRising_v1.0_1603.lha`, identical to v843.02) and
refuses program files of another build.

Keys: [keys.pdf](keys.pdf) is a one-page cheat sheet of every key (made by
`tools/docs/keys_cheatsheet.py`, which needs reportlab). F11 toggles
fullscreen, F12 captures or releases the mouse. The game is played with
keyboard and mouse.

## Windows

The Windows build is cross-compiled on Linux with MinGW (Fedora:
`mingw64-gcc-c++`, `mingw64-SDL3`):

```sh
tools/package-windows.sh        # -> dist/RedStormRising-windows.zip
```

- **The zip** holds `rsr.exe` (the C++ runtime linked in), `SDL3.dll`,
  the CRT shaders, `run.bat` (the launch screen), `keys.pdf` and a `README.txt`
  for players (`packaging/windows/`).
- **Players** put the WHDLoad archive (`RedStormRising_v1.0_1603.lha`),
  or both v843.02 disks (`.adf`), next to `rsr.exe`. On the first start the program extracts the 21 files
  it uses into `%APPDATA%\RedStormRising\data`, after checking each
  against the WHDLoad build (`src/host/game_archive.cpp`).
- **Saves** go to `%APPDATA%\RedStormRising`.

Testing uses Wine. The same scenarios give the same recordings, picture
and sound alike, and the save and recall test passes. To run the Windows
build against the scenarios:

```sh
WINEPATH='Z:\usr\x86_64-w64-mingw32\sys-root\mingw\bin' wine build-win/rsr.exe --ntsc --fast \
  --no-present --det-timing --av-hash 60 --reference-terrain --script tests/scenarios/NAME.txt DATA_DIR
```

## Linux package and releases

`tools/package-linux.sh` builds `dist/RedStormRising-linux-x86_64.tar.gz`:
`rsr` (the C++ runtime linked in, `-DRSR_PORTABLE=ON`), its own
`lib/libSDL3.so.0` (found through the program's run path `$ORIGIN/lib`),
the shaders, `keys.pdf`, `source.zip` and a `README.txt` for players
(`packaging/linux/`). It runs on distributions as old as the one it was
built on.

GitHub builds both packages (`.github/workflows`):

- **`build.yml`**, on every push and pull request. The Linux package is
  built on Ubuntu 22.04 (glibc 2.35) with SDL3 built from source; the
  Windows one is cross-compiled on Fedora with `tools/package-windows.sh`.
  It also runs the checks that need no game files: the code rules and the
  shader preset test. The gate itself needs the game's files, which are
  never uploaded, so it runs locally before pushing.
- **`release.yml`**, on a tag `v<version>`, where the version is the one in
  `CMakeLists.txt` (`project(... VERSION 0.5.0)`); a tag that does not
  match is refused. It builds both packages and makes a **draft** release
  with them, named `RedStormRising-v<version>-windows.zip` and
  `-linux-x86_64.tar.gz`, with `.github/release-notes.md` as its text.
  Tags with a suffix (`v0.5.0-rc1`) become pre-releases.

To release: set the version in `CMakeLists.txt`, merge into `main`, then
`git tag v0.5.0 && git push origin v0.5.0`. Check the draft on GitHub's
Releases page and publish it there.

## Source tree

```
src/
  main.cpp       the front end: options, SDL window, audio, input, test scripts
  launcher.cpp   the launch screen (SDL only)
  game/          the game: one directory per original program, plus the shared libraries
    platform/    what the game asks of the machine besides OS calls (input, time, frames)
    mem/         the game's view of memory: declarations of the original's data, OS calls
  amiga/         the machine: address space, display, blitter, audio, OS calls
  runtime/       the running program's context (which module, where it is loaded)
  host/          the game's files on the host (data and save directories)
  present/       optional CRT shaders (presentation only)
packaging/       the packages' readmes and run.bat (tools/package-windows.sh, package-linux.sh)
tests/           the gate, the scenarios and their recordings
tools/analysis/  analysis tools (most from the translation; check_code_rules.py is in the gate)
```
