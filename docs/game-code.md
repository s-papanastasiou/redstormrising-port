# The game code (`src/game`)

The original game is five Amiga programs. Every program was linked with
the same libraries: the C runtime and a screen, input and sound library.
The port has one directory per program and one per library. The
library code exists once, and each program has its own copy of the
library's state (see architecture.md, "The running module").

## The programs

| Directory | Program | What it does |
|---|---|---|
| `redstorm/` | RedStorm | The loader: memory check, display and fonts, preloads, then Title and RedStart / Red / Storm in turn. |
| `title/` | Title | The opening pictures and the title sequence. |
| `redstart/` | RedStart | The menus before a game: the player's name, rank and era, scenario, difficulty, the ship identification test, saved games, the hall of fame. |
| `red/` | Red | The campaign: the strategic map, the war's phases, the enemy's battle groups, orders and missions, the port and the loadout, the reports and the career. |
| `storm/` | Storm | A battle: ships, weapons and decoys, sonar and detection, damage, the tactical and other displays, the battle log and its replay, the after action report. |

### RedStorm (`redstorm/`)

| File | What |
|---|---|
| `loader.cpp` | `redstorm_main`: the game's flow. Memory check, preloads, the shared record, running a module (`run_in_loader`), the quit. |
| `display.cpp` | The libraries, the display (screen, window, bitmaps, fonts, buffers, keyboard), the data files' offset tables. |
| `state.hpp`, `vars.hpp` | Its state; its data (file names, texts, the NewScreen and NewWindow). |

### Title (`title/`)

| File | What |
|---|---|
| `title.cpp` | `title_main`: the two opening pictures and the title sequence, with their sounds. |

### RedStart (`redstart/`)

| File | What |
|---|---|
| `menus.cpp` | The first menu, the new player's choices (name, rank, era, difficulty, scenario), the identification test, the intro pages, the choices stored in the player's record. |
| `saves.cpp` | Saved games (five slots) and the hall of fame. |
| `state.hpp`, `vars.hpp` | Its state; its data (menu pages, row menus, spans, texts). |

### Red (`red/`)

| File | What |
|---|---|
| `turn.cpp` | `red_main`, `start_red`: the campaign's turn, a battle's result, the map screen's loop. |
| `campaign.cpp` | The war's phases and missions, the enemy's battle groups (speed, orders, events), scoring. |
| `frame.cpp` | The strategic map's update every frame: the units' steps, detection of the player. |
| `movement.cpp`, `routes.cpp`, `units.cpp`, `groups.cpp` | Units on the map: distances, velocities, headings, routes and patrols, forming and drawing the groups. |
| `fronts.cpp` | The front lines and how they move. |
| `map.cpp` | The player's submarine on the map (the scroll input). |
| `clock.cpp` | The campaign clock, reinforcements, small map helpers. |
| `loadout.cpp` | The port: the weapons on board, the tubes, repairs. |
| `pages.cpp` | Text pages and messages: briefings, news, the career page, medals, the campaign's end. |
| `menu.cpp` | Choosing from a text page's menu and from row menus (also RedStart's). |
| `text.cpp`, `texts.cpp` | Text on the text page (cells, control codes); the game's packed texts. |
| `record.cpp` | Taking over the player's record at the start, saving the campaign into it. |
| `state.hpp`, `vars.hpp` | The campaign's state; Red's data (tables, texts, the player's record layout). |

### Storm (`storm/`)

| File | What |
|---|---|
| `battle.cpp` | `battle_main`, the battle's start (ships placed, settings), the frame loop, the crew's questions, the log, the replay, the battle's outcome. |
| `ships.cpp` | The battle step: each ship's movement and tactics, hearing and detection (`attack_score`, `hear`), weapons and decoys (launch, seeking, homing, hits, damage), aiming and firing. |
| `panels.cpp` | What is drawn: the tactical display, the sonar, periscope, library and identification displays, the panels around them, the reports. |
| `keys.cpp` | The keys during a battle (`process_key`). |
| `text.cpp` | Storm's text and the status line. |
| `sound.cpp`, `pointer.cpp` | The engine noise; the pointer moved for aiming. |
| `state.hpp`, `vars.hpp` | The battle's state (ships, objects, projectiles, the log); Storm's data and every named constant (flags, kinds, displays, pens, effects, weapons). |

## The shared libraries

| Directory | What |
|---|---|
| `common/` | The C runtime: start-up and exit (`startup.cpp`), the allocator (`memory.cpp`), stdio's exit (`stdio.cpp`), strings and numbers (`strings.cpp`, `math.cpp`), random numbers (`random.cpp`), devices and the random seed (`devices.cpp`), the hand-over between loader and module (`module.cpp`), the shared record and the player's record (`record.hpp`, `records.cpp`), the process (`process.cpp`). |
| `screen/` | Drawing: which bitmap is drawn and shown, the palette and pointer (`display.cpp`); pens, lines, rectangles, blits and masked shapes (`draw.cpp`); text with a style (`text.cpp`, `styles.hpp`); the blitter routines (`blitter.cpp`); shapes and pictures from files or preloads (`pictures.cpp`); the picture decoder (`decoder.cpp`); the animation player and sequences (`animation.cpp`, `sequences.cpp`); alerts and disk requesters (`requesters.cpp`). |
| `input/` | Keys and mouse buttons (`input.cpp`, with the key tables' special characters in `vars.hpp`), the menu cursor (`menu.cpp`), the frame tick of RedStart and Red (`tick.cpp`), waits (`wait.cpp`). |
| `audio/` | The sound player: samples, sound programs (a small bytecode per channel, `sound_tick`), effects, the engine noise. |
| `ui/` | The state of the screen, input and menu code (one copy per module). |

## The game's interface to the machine

| Directory | What |
|---|---|
| `platform/` | `platform.hpp`: input events and the key matrix, the system time, audio requests, waits (`wait_frame`, `delay`), the vertical-blank server, routines called with their original timing (`call_timed`), the running module (`in_module`, `module_block`), the reference terrain for the tests. |
| `mem/` | `os.hpp`: the AmigaOS calls. `constants.hpp`: the programs' constant data. `globals.hpp`, `types.hpp`: declarations of the original's variables and records. `state.hpp`, `module_state.hpp`: the registry of state kept in C++ (cleared, initialised, hashed, reachable by the test scripts). `image.hpp`: struct <-> original byte layout. `register.hpp`: `ORIGINAL`. `call.hpp`: a module's main and exit. |

## Where to start reading

- **The game's flow:** `redstorm/loader.cpp` (`redstorm_main`), then
  each module's main. These are `common/module.cpp` (`storm_entry`,
  `redstart_entry`, `red_entry`) and `title/title.cpp`.
- **A battle:** `storm/battle.cpp` (`battle_main`, then `battle_step` in
  `ships.cpp`), and `storm/state.hpp` for what a ship and an object are.
- **The campaign:** `red/turn.cpp` (`red_main`, `map_screen`), then
  `red/frame.cpp` and `red/campaign.cpp`. `red/state.hpp` holds the
  campaign's state.
- **Finding a routine of the original:** search for its address, e.g.
  `0x24684` or `$24684` (see conventions.md).
