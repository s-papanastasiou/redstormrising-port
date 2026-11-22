# Architecture

## Layers

```
 +-------------------------------------------------------------+
 | front end (src/main.cpp)                                     |
 |  SDL window and audio, keyboard and mouse, test scripts      |
 +-------------------------------------------------------------+
           | Host interface: a finished frame, audio,      ^
           v input events                                  |
 +-------------------------------------------------------------+
 | machine (src/amiga)                                          |
 |  address space    display   blitter   audio channels         |
 |  scanline clock and interrupts                               |
 |  the OS calls (Hle: exec, dos, graphics, intuition, devices) |
 +-------------------------------------------------------------+
           ^ os:: calls, platform:: services, typed memory
           |
 +-------------------------------------------------------------+
 | game (src/game)                                              |
 |  RedStorm (loader)  Title  RedStart  Red  Storm              |
 |  shared libraries: common, screen, input, audio, ui          |
 +-------------------------------------------------------------+
```

- **The game** is C++ translated from the original programs. It sees
  the machine only through:
  - `game::os::` (`src/game/mem/os.hpp`): the AmigaOS calls it makes,
    with typed arguments;
  - `game::platform::` (`src/game/platform/`): input, timing, frames,
    interrupt servers;
  - the typed memory declarations in `src/game/mem/` (see
    conventions.md).
- **The machine** (`amiga::Machine`) models what the game needs of an
  Amiga 500: the address space, the display, the blitter, Paula's audio,
  and a clock counted in 68000 cycles. Its OS calls are implemented in
  C++ (`amiga::Hle`). There is no 68000 interpreter: no original code
  is run.
- **The front end** creates the machine, gives it a `Host` (window,
  audio, input), and runs it. With `--script` it feeds scripted input
  instead of the keyboard and mouse.

## A run

1. `main()` parses the options and calls `Machine::boot(data_dir)`. The
   machine's OS creates the process "RedStormRising" and loads the
   loader program, `RedStorm`, with its LoadSeg (`src/amiga/hunk.hpp`).
2. `Machine::run()` calls the loader's start-up,
   `game::common::module_startup` (registered as
   `game_hooks.program_start`), in RedStorm's context. Everything else
   happens inside that call.
3. The loader (`src/game/redstorm/loader.cpp`, `redstorm_main`):
   - checks the free memory;
   - opens the libraries and the display: the screen, the window, two
     more bitmaps, the fonts, the buffers;
   - preloads the data files the memory allows;
   - fills the record it shares with the modules;
   - runs Title once, then RedStart, Red and Storm in turn until the
     game ends.
4. To run a module, the loader:
   - loads it (LoadSeg);
   - calls its start-up (`module_startup`) with the start arguments, in
     that module's context (`platform::in_module`);
   - unloads it when it returns (unless it stays preloaded).

   The start-up clears the module's variables, opens dos.library and
   calls the module's main from a table: `storm_entry`, `redstorm_main`,
   `title_main`, `redstart_entry`, `red_entry`.
5. Modules hand over through the shared record (`common/record.hpp`):
   - RedStart leaves the player's choices there;
   - Red leaves the battle to fight (`BattleStart`) and the campaign's
     state;
   - Storm leaves the battle's outcome.

   The loader reads the result and picks the next module.

### The running module

Each original program had its own copy of the shared libraries (screen,
input, audio, the C runtime) and its own variables. The port keeps one
copy of the code and **one copy of the state per program**:
`ui::state()`, `audio::state()`, `common::state()` and the others
return the running module's copy.

The running module is `runtime::module_index()`, in the order storm 0,
redstorm 1, title 2, redstart 3, red 4. The loader sets it for a module
it runs (`platform::in_module`), and an interrupt server runs in the
module that installed it.

## Time and frames

The game drives time, as on the Amiga: code runs and the clock advances
by what it does.

- **A cycle count** (`Machine::cpu.cycles`, in 68000 clock cycles) is
  the emulated time. Every OS call takes the time the original's call
  took (`Machine::os_call`): 12 cycles, plus a scanline for a call that
  waits. A routine the original called through an address takes the
  time of that call (`platform::call_timed`).
- **Scanlines.** When the count passes a scanline the machine runs
  that line: audio, interrupts. A frame is 262 lines (NTSC) or 312
  (PAL). At its end the picture is rendered, the vertical-blank
  interrupt is raised, and the host gets the frame.
- **Waits.** `wait_frame`, `delay` and waits for audio go through
  `Machine::idle_until`: lines pass, with interrupts, until the
  condition holds.
- **Interrupt servers.** The game installs its vertical-blank server
  (the sound player's tick and the frame counter) with
  `platform::add_vblank_server`.
- **Deterministic timing.** With `--det-timing` only the waiting calls
  take time. Runs are then reproducible, and the tests compare them.

The front end paces the frames to real time (50 Hz for PAL, the
default, 60 Hz for `--ntsc`), unless it runs with `--fast`.

### Additions to the picture

Two things are drawn on each finished frame, after the display is
rendered (`Machine::end_of_frame`), never into the game's bitmaps:

- **The game's frame overlay** (`GameHooks::frame_overlay`, set through
  `os::set_frame_overlay`): the detailed map's port marks
  (`src/game/red/map_marks.cpp`, `--detailed-map`: anchors and names at
  the briefings' ports, Spitsbergen's name). A mark pixel is drawn only
  where the shown bitmap has the clean map's pen (bitmap 2) at that point,
  so units are drawn over the marks, as if the marks were part of the
  map's picture. The game reads its map's pixels for play (land and sea,
  the fronts), which is why the marks never go into the map itself.
- **The mouse pointer** (`Hle::draw_pointer`), on top of everything.

### PAL and NTSC

The video standard: `--pal` (the default: a European Amiga, 312 lines at 50
Hz) or `--ntsc` (262 lines at 60 Hz). The game counts frames for its
pictures and game timing but plays its music by real time (on PAL its
vertical-blank handler adds a music tick every 6th frame), so the intro
animation is 20% slower on PAL against nearly the same music, as on a
European machine. On PAL the game opens its screens 10 lines lower; the
200-line window follows them, so both standards show the same picture.
The tests run as NTSC (the scenarios' frame numbers were taken on it).
Frames are paced by the sound card: the audio queue is held about 30 ms
above one frame's audio (`RSR_AUDIO_DEBUG=1` shows it).

### The CRT shaders

`--crt-guest`, `--crt-guest-fast` (crt-guest-dr-venom),
`--crt-lottes-fast` and `--crt-royale` (bundled libretro shaders, OpenGL
3; licences in `data/shaders/libretro/README.md`), or `--shader-preset
FILE` for any libretro GLSL preset; `src/present`, from fa18. They are
presentation only: errors fall back to the normal output.
`tests/check_shader_present.sh DATA_DIR` checks them.

- **Cost.** At 4K on a small integrated GPU (Radeon 610M) crt-royale
  costs about 20 ms per frame: the game then runs below 60 fps and its
  audio crackles. crt-guest, crt-guest-fast and crt-lottes-fast hold 60
  fps (most Guest passes run at the source resolution). On a discrete GPU
  all hold 60 fps. crt-lottes itself (about 37 ms there) is not bundled.
- **History.** The runner keeps the last 8 source frames for presets that
  sample PrevTexture/Prev1-6Texture (Guest's afterglow).
- **Hybrid graphics.** With a monitor on the integrated GPU, render on
  the discrete one with Mesa PRIME offload, e.g.
  `DRI_PRIME=pci-0000_03_00_0`.
- `RSR_AUDIO_DEBUG=1` reports frames/s, the audio queue and the shaders'
  render and swap times.

## The address space

The original's data has addresses, and the game still names some
things by them. `Machine::bytes(address, n)` resolves an address to host
memory:

| Range | What |
|---|---|
| below `$20000` | low memory: the OS's structures (library bases, the process) |
| the programs' images | each loaded program in a host block at its load address: its code (read as data in places), its data, its BSS |
| above that | the heap: the places the original allocated (pages made on first use) |
| from `$800000` | the arena: chip memory (bitplanes, samples) and file data, at the heap place the original allocated |

The heap keeps the original's layout, because the game depends on it in
one place. Storm's sea bed is its own program code read as terrain
(`storm::area_level`), so the terrain depends on where Storm was loaded.
The game uses the terrain of wherever Storm is loaded. The tests take it
as the recordings had it (`--reference-terrain`, see testing.md).

Host objects named by an address:

- bitmaps: `screen::bitmap(address)`, `amiga::BitMap`;
- rastports: `RastPort`;
- screens and windows: `Hle::screen`, `Hle::window`;
- text styles: `screen::style_at`.

The address is where the original kept the object's bytes. It is what
the game stores and passes, and the host object is looked up by it.

## The machine's files

| File | What |
|---|---|
| `machine.hpp/.cpp` | The machine: address space, the program's start, the scanline clock, the frame's end. |
| `cpu.hpp` | What is left of the 68000: a4 (the running module's data base), d0, the interrupt mask, the cycle count. |
| `modules.cpp` | The loaded programs: the file check, the test scripts' set/get by original offset, the OS calls' time, the memory hashes. |
| `hunk.hpp` | The Amiga program file format and LoadSeg's layout (`HunkFile::image_at`). |
| `hle.hpp/.cpp` | The OS calls: exec, dos, graphics (palettes, screen bands), keyboard and timer. |
| `intuition.cpp`, `intuition_objects.hpp` | Screens, windows, the pointer, input events. |
| `gfx_draw.hpp/.cpp` | graphics.library drawing: text, lines, rectangles, blits between bitmaps. |
| `audio_device.cpp` | audio.device on the four channels. |
| `custom.cpp`, `blitter*.hpp` | The display, the blitter, Paula, the mouse. |
| `bitmap.hpp`, `rastport.hpp` | BitMap and RastPort as host objects. |
| `tasks.cpp` | The process's exit (the program's end). |
