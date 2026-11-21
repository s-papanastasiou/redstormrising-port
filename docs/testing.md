# Testing

The port has to play exactly like the original. The tests record what it
shows and plays, frame by frame, and every change must keep that the
same.

## The gate

```sh
bash tests/check_native.sh WORKDIR    # the last line must be PASS
```

Run it before every commit. It:

1. builds, and checks the code rules (`tools/analysis/check_code_rules.py`);
2. runs every scenario twice and compares the memory hashes
   (determinism);
3. in the first run, traces the game's memory accesses and fails on any
   of these:
   - `STATE MOVED`: emulated memory reached for state that lives in
     C++;
   - `CONST CHANGED`: constant data that was written;
   - `[io]`: an access outside RAM;
4. runs the scenarios in a `-fsanitize=bounds` build (`build-bounds/`):
   no index outside a C++ array;
5. compares each scenario's picture and sound with its recording;
6. saves a game and recalls it (`tests/check_save_load.sh`).

All runs use `--det-timing` (only waiting OS calls take time), as an
NTSC Amiga, with `--reference-terrain` (below).

## Scenarios

`tests/scenarios/*.txt` are scripts of input by frame number, played
with `--script`. Each covers part of the game:

| Scenario | What it covers |
|---|---|
| `redstart_menus` | RedStart's first menu: the saved games list, the hall of fame. |
| `campaign` | A new campaign: the opening pages, the orders, leaving port, the strategic map. |
| `campaign_base`, `campaign_status` | The submarine at its base; the status report (weapons, tubes, damage). |
| `campaign_battle` | From the campaign into a battle and back: sonar contact, firing a Mk48, the replay, the after action report. |
| `campaign_flags`, `campaign_group0`, `single_mission_end` | `campaign_battle` with variables set for rarer branches: a mission's end check, the battle as group 0's, the battle as a single mission. |
| `battle_training`, `battle_ice`, `battle_kashin` | Training battles: against a November-class submarine (in open water, in drift ice), against a Kashin-class destroyer. |
| `battle_periscope` | The training battle, then the sea conditions, the periscope, an ordered depth. |
| `disk_requesters` | The insert-disk requesters (`disks out`). |

Script lines (the full list is at the top of `src/main.cpp`):

```
2450 mouse 0 -75        move the mouse
2470 click              press and release the left button
3150 key T              a key (an SDL key name or a hex Amiga raw key; down, up or tap)
4200 lmb 1              left button down (0: up)
8650 set red a4+14908 2 190   (tests only) store a value in a game variable
+20 get storm a4-30936 2      log a variable's value
53000 quit
```

A frame may be `+N`, counted from the previous line. `set` and `get`
name a variable by its original offset; the state tables route them to
the C++ field (see conventions.md).

`tests/run_scenarios.sh OUTDIR [options]` runs them all in parallel,
headless. Each writes `OUTDIR/<name>.log` and a screenshot of its last
frame.

## Recordings

`tests/reference/<scenario>.txt` holds, for every 60 frames, a digest of
the picture and one of the sound:

```
[avhash] frame 60 video 2d8f5df0b9a35121 audio d1b31d11502a2e3a
```

- `tests/check_reference.sh` compares a build with them.
- `tests/record_reference.sh` records them again. Do that only for a
  change meant to alter the game, after looking at it, and say so in the
  commit.

### The reference terrain

Storm's sea bed is its own program code, read as terrain, so it changes
with the address Storm is loaded at.

- **`--reference-terrain`** takes those bytes from Storm's file as if it
  were loaded where the recordings had it (`$09e4f8`), wherever it is
  loaded now. A change to the memory layout then changes nothing else,
  and the recordings still check everything.
- **Without the option**, the game plays with the terrain of where Storm
  really is.

### PAL

The scenarios were recorded as NTSC. PAL (the default) is checked by
comparing a build's PAL runs with an older build's (`--av-hash 60`
without `--ntsc`, same scripts).

## Debugging aids

| Environment variable | What |
|---|---|
| `RSR_TRACE=1` (run_scenarios.sh) | Writes the trace of the game's memory accesses (`RSR_STATE_TRACE=FILE`), read by `tools/analysis/statemap.py`. |
| `RSR_TRACE_STOP=MODULE:OFF` | Stops (for a debugger) at the first access covering a4+OFF of a module (0-4). |
| `RSR_RAM_DUMP=FRAME,FILE` | Writes memory at the end of that frame. |
| `RSR_DATA_WRITES=FILE` | Lists the program images' bytes that changed while loaded. |
| `RSR_LAUNCHER_SHOT=FILE.bmp` | Saves the launch screen's first frame and quits (with `SDL_VIDEODRIVER=offscreen`, no display needed). |
| `RSR_LAUNCHER_KEYS=up,right,return` | Key presses fed to the launch screen first (tests of its choices). |
| `RSR_DATA`, `RSR_BIN` | The game data and the binary for the test scripts. |
| `RSR_AUDIO_DEBUG`, `RSR_SHADER_DEBUG`, `RSR_PRESENT_SHOT`, `RSR_SHADER_DIR` | Audio and shader diagnostics; where the bundled shaders are. |

Options for comparing runs: `--ram-hash N` (memory hashes), `--av-hash N`
(picture and sound digests), `--screenshot N FILE`, `--frames N`, `--fast`
and `--no-present`.
