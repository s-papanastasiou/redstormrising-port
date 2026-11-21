# Red Storm Rising: reverse-engineering notes

What was found out about the Amiga game while porting it: the versions
of the game, its files, the OS calls and hardware it uses, how its
programs hand over to each other, the copy protection and the saves.
Most of it was written at the start (2026-11-10) and is kept as it was
then; dated sections say when. How the port itself works is in
[architecture.md](architecture.md).

## The starting approach (2026-11-10)

(History: the 68000 interpreter and the recompiler were removed once all
of the game was C++; see `history/restructure-strategy.md`.)


That project runs the game's original 68000 code instead of rewriting it:

1. **Execute the original code.** A 68000 interpreter (`src/m68k/`) and a
   static recompiler (`tools/recomp.py`, 68000 → C++ at build time) run the
   unmodified executable, bit-identically.
2. **High-level Amiga runtime** (`src/amiga/`): emulate only the AmigaOS
   library calls the game makes (`hle.cpp`: exec/dos/graphics/intuition and
   devices) plus the custom chips it programs directly (copper/bitplanes,
   blitter, Paula, ports), driven by a scanline scheduler (PAL or NTSC).
3. **Copy protection** bypassed exactly as the game's WHDLoad slave does.
4. **Gradual readable translation** of routines into C++ (`src/native/`),
   gated by RAM-hash equivalence against the original code
   (`tests/check_native.sh`), then remaster options on top (OpenGL etc.).

Reusable as-is or nearly so: the 68000 interpreter and recompiler, the ADF
reader (`tools/adf.py`, `src/amiga/adf.hpp`), the hunk loader tools
(`tools/hunks.py`, `tools/loadseg.py`, `tools/disasm.py`) and most of the
custom-chip model. Game-specific work: the set of OS calls/hardware features
used, protection bypass, file layout, and any timing assumptions.

## Source material (local only, in `original/`, git-ignored)

**Never commit game data.** `original/` is ignored by `.gitignore`.

| File | SHA-256 |
|---|---|
| `Red Storm Rising v843.02 (1990)(MicroProse)(Disk 1 of 2)[cr SR].adf` | `c0b6dfb117f9e2cc37092b483d16cc6188e69ef7c03409d7aa1ed1fbf577d001` |
| `Red Storm Rising v843.02 (1990)(MicroProse)(Disk 2 of 2)[cr SR].adf` | `9e1515c55a83f4fdb9f0771f95c068c5405597246046f2c8ce68f7639f6f40c1` |
| `RedStormRising_v1.0_1603.lha` (WHDLoad install) | `9b5d92730c13faaa848bedd77ddfee4982c68d8f46f0b3863c59e7dd87affade` |

## The two versions (2026-11-21)

The first notes below were made on the **v843.01** disks (Skid Row
crack), since removed from `original/`. **v843.02** ("Amiga Red Storm
Rising, Version 2", its ReadMe dated 6 April 1992; also a Skid Row crack)
is the build the WHDLoad install holds:

- All five programs and every data file on the v843.02 disks are
  byte-identical to the WHDLoad install's, except two bytes of `RedStart`
  (file offset `$2f8`, code `$2d4`): the crack's `4E 71` (NOP) where the
  original has `67 68` (`beq`), the same spot the WHDLoad slave patches.
- v843.01 has the same data files but all five programs differ (another
  compilation of the code), so the port cannot run it.
- Nothing published describes what changed between the two (searched
  2026-11-21).

The port reads the v843.02 disks directly (`src/host/game_archive.cpp`:
an OFS/FFS reader); it puts `RedStart`'s two bytes back, so the files
equal the WHDLoad install's and `--protection` works with them too.

## First observations (2026-11-10, on v843.01)

- **Both ADFs are standard OFS file disks** (readable with
  `../fa18-interceptor-port/tools/adf.py list`), not trackloaded: the game
  loads through AmigaDOS like F/A-18 Interceptor, which is the favourable
  case for this approach.
  - Disk 1: boot disk with `c/`, `l/`, `s/startup-sequence`, fonts
    (`RedStorm1-3.fnt`), `TITLEBM`, `titlescr`, `*_index` files, install
    scripts (`RSR_HD_Install`, `s/RSRInstall2`), `CreateSaveDisk`.
  - Disk 2: executables/data `Red`, `Redstart`, `Storm`, `ATBMS`, `ATSCR`,
    `ships`, `osg`, `sounds`, `pics`.
- **The ADFs are a Skid Row crack** (`[cr SR]`, `s/SKIDROW` on disk 1).
  The cracked executables differ in size from the WHDLoad archive's copies
  (`Red` 125212 vs 125232, `Storm` 111156 vs 111208, `Redstart` 83480 vs
  83512 bytes), so they are modified. Try to obtain an uncracked (TOSEC
  original) dump; the WHDLoad slave's patch offsets target specific original
  files, as fa18's codewheel bypass did.
- **WHDLoad archive** (`RedStormRising_v1.0_1603.lha`, slave v1.0,
  files dated 2006-01, 56 entries): a "files" install with `RedStormRising.Slave`
  (4504 bytes), `ReadMe`, `Manual` and a `data/` directory holding
  executables `Red`, `RedStart`, `RedStorm`, `Storm`, `Title` plus data
  files (`pics`, `ships`, `osg`, `sounds`, `opening`, `more`, fonts,
  `*_index`, `saves/`). The slave's 68000 source/binary documents what the
  game needs (OS calls patched, protection, quit key, save handling).
  Extracted into the ignored `extracted/` (`7z x -oextracted ...`).

## Survey of the WHDLoad install (2026-11-10)

- **Kickemu install:** the slave needs a real Kickstart 1.3 ROM image
  (`kick34005.A500` + `.RTB`): WHDLoad boots the actual OS around the game
  instead of replacing it with patches. 0.5 MB chip + 1.0 MB other memory.
  Manual protection "removed (thanks SkidRow)", saves to `saves/`. The
  ReadMe says source code is included, but the archive has only the binary
  `RedStormRising.Slave` (4504 bytes).
- **An OS-friendly, multitasking program** (game ReadMe: "will multi-task
  happily"); it caches code/sounds/pictures depending on free memory
  (<420K refuses, >650K code, >1040K everything), needs ~240K chip RAM.
- **OS interface** (strings in all executables): `dos`, `graphics`,
  `intuition`, `diskfont` libraries; `audio`, `keyboard`, `timer`, `input`
  devices. Same set as fa18 plus `diskfont.library` (three game fonts
  `RedStorm1-3.fnt`, loaded by name).
- **Volumes:** files are opened as `RedStormA:` (disk 1), `RedStormB:`
  (disk 2) and `RedStormSave:` (save disk; files `Game`, `Fames`). The HD
  install assigns all three to one directory. HLE dos needs these assigns.
- **Program structure** (`../fa18-interceptor-port/tools/hunks.py`):

  | File | Hunks | Role (from strings) |
  |---|---|---|
  | `RedStorm` (29948 B) | CODE 15272, DATA 3988, DATA 10708 | resident core/loader: "LOADING CODE...", "Program load failure"; loads the modules below, data indexes, fonts |
  | `Title` (41948 B) | CODE 22276, DATA 37772 | title screen module (`TITLEBM`, `TITLESCR`) |
  | `RedStart` (83512 B) | CODE 35180, DATA 63716 | game setup module |
  | `Red` (125232 B) | CODE 61840 + 11 DATA/BSS hunks | game module |
  | `Storm` (111208 B) | CODE 83044, DATA 42984 | game module (also opens `input.device`) |

  Modules are loaded from `RedStormB:` (`title` from `RedStormA:`) on
  demand, i.e. LoadSeg-style swapping, not one big executable as in fa18.
  No symbols in any hunk. `more`, `c/*`, `libs/*` are stock AmigaOS files.
- Data files: `pics`, `ships`, `osg`, `sounds`, `ATBMS`, `ATSCR`,
  `opening`, `TITLEBM`, `titlescr` with `*_index` files.

## OS calls and hardware access (2026-11-10)

`tools/oscalls.py --fd FD_DIR EXE...` (uses fa18's hunk loader unpatched and
`m68k-linux-gnu-objdump` from Fedora `binutils-m68k-linux-gnu`; NDK .fd
files from amitools `amitools/data/fd`, kept in ignored `extracted/fd`)
resolves every `jsr -N(a6)`. Full per-module list with call sites:
`extracted/oscalls.txt` (regenerate; not committed).

- **Aztec C (Manx)** start-up in every module (`lea DATA,a4`, small-data
  library bases at `a4+N`); each module is a complete program that opens
  its own libraries. `RedStorm` copies its bases into a shared table
  (`a4@(10026)`: graphics, intuition, diskfont, ...) - presumably state
  handed to the modules; to be confirmed.
- **76 distinct library calls**, ~45 already implemented by fa18's HLE
  (`src/amiga/hle.cpp`). New ones:
  - dos: `LoadSeg`, `UnLoadSeg`, `CreateProc` (module loading/starting).
  - graphics (14): `Text`, `TextLength`, `SetFont`, `SetAPen`, `SetBPen`,
    `SetDrMd`, `Move`, `Draw`, `RectFill`, `SetRast`, `ReadPixel`,
    `GetRGB4`, `BltBitMap`, `BltMaskBitMapRastPort` (RastPort drawing).
  - intuition (13): `OpenScreen`, `CloseScreen`, `OpenWindow`,
    `CloseWindow`, `MakeScreen`, `RethinkDisplay`, `ScreenToFront`,
    `ShowTitle`, `SetPointer`, `ActivateWindow`, `OpenWorkBench`,
    `CloseWorkBench`, `DisplayAlert`, `LockIBase`/`UnlockIBase`.
  - diskfont: opened but **never called**; the game reads its `.fnt` files
    itself (fonts set with `SetFont`).
  - devices: `BeginIO` direct to devices (io_Device at IORequest+20).
- **Direct hardware:** only a blitter library linked into every module
  (`DMACONR` busy polling, `BLTxxx` $dff040-$dff074, `BLTSIZE`, `DMACON`),
  plus one read of `JOY1DAT` ($dff00c, joystick port) and CIA-A `$bfe001`
  (fire) per module. The mouse arrives as IDCMP messages, and the cursor
  keys steer on the strategic map, so no joystick is needed. **No
  Paula and no copper writes:** the display is an Intuition screen and all
  sound goes through `audio.device`, so the HLE needs real `audio.device`
  playback (fa18 only allocates channels; it drives Paula directly).

## Module protocol (2026-11-10)

Addresses: runtime addresses of fa18's loader layout (first hunk code at
$20008), WHDLoad `data/` files.

- **`RedStorm` is the launcher.** Its process must be named
  `RedStormRising`. It allocates a 164-byte shared table (`a4@(10026)`),
  fills it at `$204b6` (graphics, intuition, diskfont bases, screen/window
  pointers, ...) and publishes it in its own task's `tc_UserData` (offset
  88): `Forbid; FindTask(NULL)->tc_UserData = table; Permit` at `$20682`.
- **Modules find it by task name:** each module does
  `Forbid; FindTask("RedStormRising")->tc_UserData; Permit` (Title
  `$20228`, RedStart `$213c8`, Red `$24b5c`, Storm `$21540`).
- **Launch = Workbench-style start** (`RedStorm` `$222cc`/`$2232c`/
  `$223e6`/`$2241c`): `LoadSeg("RedStormA:title"...)`; `Forbid`;
  `CreateProc(name, pri 0, seglist, stack 4000)`; `Permit`;
  `Lock(dir, ACCESS_READ)`; build a 40-byte `WBStartup` message
  (`ln_Name "WAKEUP"`, `sm_NumArgs 1`, `sm_ArgList` = {lock, name},
  reply port) and `PutMsg` it to the new process; then `WaitPort`/`GetMsg`
  on the reply port until the module exits; `UnLock`; `UnLoadSeg`. Modules
  run one at a time while the launcher waits.
- **Fonts are LoadSeg'd, not opened with diskfont:** `RedStormA:RedStorm1-3.fnt`
  are segments whose code starts with a `DiskFontHeader`; the `TextFont`
  is at seglist*4 + 8 + 54 (`$21096`-`$2112e`). Then `SetFont`.
- **HLE consequences:** LoadSeg of arbitrary hunk files at runtime (game
  modules and fonts), a second process (CreateProc + WBStartup + reply)
  with cooperative switching at `WaitPort`/`Wait`, named tasks for
  `FindTask(name)`, `tc_UserData`.

## Copy protection (2026-11-10)

- The **WHDLoad `data/` files and the cracked ADFs are different builds**,
  not original vs patched: the code differs in compiler output
  (e.g. RedStorm `moveq #10,d0; not.b d0` vs `move.l #260,d0`) and Title
  has extra code blocks. Diffing them cannot isolate the crack.
- The **slave** (`RedStormRising.Slave`, kickemu for Kickstart 1.3
  34005.a500, 512K base + 768K exp mem, quit key `$5d`, dir `data`,
  volumes `RedStormA`, `RedStormB`, `RedStormSave`→`saves`) hooks
  LoadSeg; for the file `RedStart` only it applies one patch list (slave
  offset `$fd2`): `PL_W $2d4,$4e71`, i.e. **NOP the word at RedStart code
  offset $2d4** (`$202dc`: `beq.s $20346` right after `bsr $20d72`). So
  `$20d72` is the manual-protection question; the NOP makes every answer
  pass (the success path sets `*(a4@(28646)) = 1`). A port applies the same
  patch to the WHDLoad `RedStart` (like fa18's codewheel bypass).
- The rest of the slave is JOTD's standard kick13 emulation (kickstart
  patch list at slave `$70`).

## Saves (2026-11-10)

`RedStormSave:` maps to the save directory (default
`~/.local/share/redstormrising`, on Windows `%APPDATA%\RedStormRising`, or
`--save DIR`). The game writes a 400-byte index `Games` and one `Game<N>`
per slot (500 bytes), and checks the volume with `Lock("RedStormSave:")`.
Saving: in a mission, "Your Orders, Sir?" -> Computer Log -> SAVE. Recall
from the main menu (the warship identification question is asked again).
`tests/check_save_load.sh DATA_DIR` covers both. The honour roll
(`Fames`) is not tested.
