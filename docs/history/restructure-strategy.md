# Strategy: from verified transliteration to a readable, restructured game

> Moved from the root (`readme-restructure-strategy.md`) on 2026-11-21.
> `readme-translate-to-cpp.md`, the first translation plan it names, was
> deleted then; it is in the git history (before that date).

Written 2026-11-12 at commit `536912f`. This document replaces the working
method of `readme-translate-to-cpp.md` from here on. That file stays as the
record of tools, lessons and history; where the two disagree, this one wins.

Read this whole file before doing any work. Section 9 lists the order of
work; section 11 lists when to stop and ask the user.

## 1. The goal

A readable, restructured Red Storm Rising in C++: named variables and
records, ordinary functions with parameters and return values, ordinary
calls, one program instead of five Amiga modules, no 68000 interpreter, no
emulated Amiga underneath the game logic.

"All routines are native" is **not** the goal. It is a milestone on the way.

## 2. Why the method changes

Measured at `536912f`:

- 64% of the game's instructions (45,201 of 70,448) have a native version:
  Storm 57%, RedStorm 43%, Title 69%, RedStart 61%, Red 76%.
- The 21,800 lines in `src/native` are assembly written in C++ syntax:
  `set_w(0, ...)`, `rd16(a4 + 15102)`, hand-tracked pushes and pops, exact
  condition codes. Continuing this way to 100% produces an Amiga program in
  C++ syntax, and a second full pass would still be needed to make it
  readable.
- Verification is thinner than "the gate passes" suggests. Of 433 native
  functions, 44 are never executed by any scenario; of 856 hooks, 281 are
  never executed; only 100 functions are compared per call. Everything else
  relies on RAM hashes matching along the paths four scenarios take.
- The per-call verifier already ignores d1, a0 and a1 for C-convention
  routines. The exact scratch registers in recent translations bought
  nothing; only the condition codes and the full d0 are still compared.

So: stop writing register-exact code, strengthen verification, and write
(and rewrite) routines directly in readable form while the original is
still there to compare against.

## 3. What stays

- The original routine is the specification. Every change is checked
  against it by `tests/check_native.sh`. Never commit with a failing gate.
- Native code replaces original routines in place, so the game stays
  playable at every commit.
- State stays in the emulated memory image until section 10. That is what
  lets translated and untranslated code run together and be compared.
- The tools in `tools/analysis` and the lessons in
  `readme-translate-to-cpp.md` (module copies, a4 offset shifts, jump
  tables, `call_lib` takes a positive LVO).

## 4. Three levels of code

| Level | What it is | Where |
|---|---|---|
| L0 | Original 68000 routine, interpreted | game data files |
| L1 | Register-exact transliteration (all work so far) | `src/native/` |
| L2 | Readable C++ working on the emulated memory image | `src/game/` (new) |
| L3 | Readable C++ with its own state, no emulated memory | section 10 |

**New work is written at L2.** L1 code is converted to L2 subsystem by
subsystem and deleted from `src/native`. Progress is `src/native` shrinking
and `src/game` growing.

## 5. Rules for L2 code

An L2 function must read like code a C programmer would have written for
this game in 1989, with modern types.

1. **No CPU state.** No `cpu()`, no `c.d[n]`, `c.a[n]`, flags, `push_w`,
   `pop`, `self()`, return offsets, or reproduced prologues. The adapter
   (`NATIVE_C`'s `c_adapter`, or its successor) reads the arguments and
   stores the result; the function body never sees a register.
2. **No numeric addresses or offsets in function bodies.** Every a4
   offset, absolute address and record field offset appears exactly once,
   in a declarations header, with a name and a one-line comment. Bodies use
   the name. If the meaning is unknown, give a placeholder name that says
   what is known (`campaign_word_14606`) and rename it when understood.
   This rule is what makes section 10 possible; do not bend it.
3. **Real parameters, locals and return values**, with the narrowest
   correct type. Aztec C `int` is 16 bits: use `int16_t`/`uint16_t` and
   cast back after arithmetic wherever the original wraps at 16 bits (C++
   promotes to 32-bit `int`). The gate will catch a missed wrap only on a
   path a scenario takes, so think about it on every expression.
4. **Records are struct views**, not offset arithmetic: `ship.heading`,
   `window.left`, not `rd16(p + 2)`.
5. **Calls are ordinary calls.** An L2 function calls another routine by
   its C++ name. If the callee is not L2 yet, the name is a thunk that
   calls the original (section 7, task T5). No addresses at call sites.
6. **Repeated compiler idioms become one helper.** The original inlines
   things like "set the text column" (store the column, compute the pixel
   position from the window's left edge) dozens of times. Write the helper
   once and call it.
7. **Control flow is structured**: `for`, `while`, `switch`, early
   `return`. No emulation of branch order beyond what changes behaviour.
8. **One function for all module copies.** Per-module differences live in
   the declarations (named variables that resolve per module), never as
   `by_module({...})` tables inside bodies.
9. **Comments say what the routine does in game terms**, plus the original
   address(es). Keep the semantic comments already written for L1; drop the
   prologue/stack notes.
10. **A local whose address is passed to the OS or to an L0/L1 routine**
    must live in emulated memory: use the scratch allocator from task T4.
11. **Busy-wait loops** (waiting for a counter the interrupt changes) keep
    the existing `native_spin` mechanism so deterministic timing works.

Illustration only (the names are invented; the shape is the point).
L1 today:

```cpp
set_w(0, rd16(a4 + 14586));
set_w(1, rd16(a4 + 14584));
c.d[2] = c.d[1];
set_w(2, uint16_t(c.d[2] + c.d[0]));
if (int16_t(c.d[2]) >= 13) { set_w(1, uint16_t(c.d[1] + c.d[0] - 13)); c.d[2] = c.d[1]; }
else c.d[2] = 0;
...
wr16(a4 + 15102, col);
set_w(0, uint16_t((rd16(a4 + 15102) << 3) - rd16(rd32(win) + 2)));
c.a[0] = rd32(win);
wr16(c.a[0] + 18, uint16_t(c.d[0]));
```

L2 target:

```cpp
// Red $2d75e: the strength gauge under the status line.
void draw_gauge_row() {
  const int16_t lit = std::max<int16_t>(0, int16_t(force_a + force_b - 13));
  set_text_cell(0, 24);
  print_styled(gauge_label_left, Style::Dim);
  ...
  for (int16_t block = 10; block > 0; --block)
    print_styled(gauge_block, lit >= block ? Style::Bright : Style::Dim);
}
```

12. **OS calls** go through `game::os` (`src/game/mem/os.hpp`): typed
    wrappers that load the registers and restore a6. Add a wrapper there
    when a new call is needed.
13. **Routines in several modules** are registered once per module copy
    with `GAME_L2(fn, storm, redstorm, title, redstart, red)` (0 where a
    module has none; `src/game/mem/register.hpp`), or `NATIVE_L2` /
    `NATIVE_L2_ALSO`; the address list is the only place the copies
    differ. The analysis tools read registrations through
    `tools/analysis/registrations.py`.
    Registration lines are exempt from the numeric-literal check.

**Exception:** routines hand-written in assembly whose callers depend on
registers or flags (task T1 lists them; `mulu32` is one) stay L1 until
their callers are L2, then get a plain C++ replacement.

## 6. Verification model for L2

Two gates, both already in `tests/check_native.sh`:

- **Whole-program equivalence** (native off vs on, RAM hashes and
  screenshots, deterministic timing). Unchanged. This is the primary gate.
- **Per-call verification**. To be widened by tasks T1 and T2 so that it
  covers nearly every executed routine, and relaxed so that it compares
  only what callers can observe.

Consequences to keep in mind:

- An L2 function that calls another L2 function directly does not pass
  through the callee's hook. The callee is still verified per call when the
  original code calls it, and as part of its L2 caller.
- To bisect a difference, `RSR_NATIVE_SKIP=name` disables a routine's hook;
  its direct L2 callers must be skipped too.
- An L2 routine that has no direct caller (reached only through function
  pointers or jump tables, or dead in a module) is not in the liveness
  table. Its callers are compiled C, which reads only d0, so it is
  verified on its declared result only (decided in the audio pilot: 40 of
  the audio registrations are like this).
- The RAM hashes leave out the process stacks entirely. L2 code does not
  reproduce the original's pushes and locals, so stale words differ even
  inside live frames (an uninitialised local in a later frame showed a
  return address in one run and the trampoline in the other). What the
  game keeps on a stack and uses shows up in the hashed memory anyway.
- **Code no scenario executes is unverified.** Task T3 makes that visible.
  Do not convert or translate an unexecuted routine without first adding a
  scenario that reaches it, unless it is trivially small. If that is not
  practical, leave it as it is and put it on the list in T3.

a6 is not compared for L2 routines: hand-written originals (the blitter
recolouring of Red $285e8) leave a library base in it, compiled callers
load a6 before each library call and never read it otherwise, and L2 code
cannot leave anything in it (its OS calls restore it).

## 7. Infrastructure to build first

Do these in order, one commit each (more if natural). Each has an
acceptance check; do not start L2 conversion before T1-T5 are done.

### T1. Caller liveness tool, then relax the verifier

`tools/analysis/liveness.py`: for every routine, look at every call site
(direct `jsr`/`bsr`, including through jump stubs) and scan forward from
the return address until each of these is overwritten or read: the
condition codes, d0 low byte / low word / high word, d1, a0, a1. Report
per routine what any caller reads. Treat a routine whose address is taken
(function pointer, jump table) or that is reached by `jmp` as "everything
live".

- Output a checked-in table (for example `src/native/live_after_call.inc`)
  of routines where something beyond the declared result is live.
- `verify_native` (`src/amiga/native_dispatch.cpp`) then compares the
  condition codes only for routines in that table, and compares d0 only
  to the width the callers read.
- The gate fails if a routine registered as L2 is in the table.

Acceptance: the gate passes unchanged for all current code; the tool's
table is short and consists of hand-written assembly helpers; a
deliberately wrong flag in an L1 routine not in the table no longer fails
verification, a wrong result still does.

### T2. Per-call verification of routines that call the OS

Today `verify=false` excludes every routine with an OS call: 690 of 973
registrations. Most of those calls only draw into emulated bitmaps, which
the snapshot already covers (forced verification with
`RSR_FORCE_VERIFY` already works for drawing routines).

- Classify HLE library calls as *repeatable* (all effects are in emulated
  RAM or in the snapshotted custom-chip state) or not (waiting, timing,
  file I/O, memory allocation, audio device I/O, input).
  Start with a conservative allowlist of graphics drawing calls and check
  each one's HLE implementation for host-side state before adding it.
- During the reference run, a non-repeatable call aborts the verification
  of that call (restore the snapshot, count it as skipped, run the native
  version), like the existing heap high-water check.
- Verify every routine by default; the registration flag becomes an
  opt-out for special cases.

Acceptance: the gate's "calls verified" and "functions verified" numbers
rise substantially with zero mismatches (investigate every mismatch: it is
either a real bug in an L1 translation or a call wrongly classified as
repeatable); a deliberately injected drawing bug is reported as a
MISMATCH naming the routine.

### T3. Coverage report and more scenarios

- Record which original instructions execute in the `--native off` runs
  (a bitmap of program counters per module, written at exit under an
  environment variable).
- `tools/analysis/coverage.py`: per routine, instructions executed /
  total, across all scenarios; list native routines with no or partial
  coverage; print module totals in the gate output.
- Add a test-only script command to set game variables (module-qualified
  a4 offset or absolute address, value) so scenarios can reach states that
  take hours of play: medals, promotion, end of campaign, damage, each
  weapon type. Both the off and the on run execute the same script, so
  equivalence still holds. Also check whether game saves placed in the
  `--save` directory can start a scenario late in a campaign.
- Add scenarios in new files (never insert keys into existing ones) until
  the 44 unexecuted native functions are either covered or listed in a
  section "Unverified" at the end of this file with the reason.

Acceptance: the gate prints coverage per module; the unverified list
exists and is short.

### T4. Typed access to the memory image

In a new `src/game/mem/` (names are suggestions):

- Big-endian field types (`be16`, `be32`, signed variants, byte arrays)
  so that a plain struct can be laid over emulated RAM, and `Ptr<T>` for a
  stored 32-bit emulated address with `->`.
- Named globals: one header per module group declaring every variable
  used, on top of the existing `Global`, `LibVar` and `global_by_module`
  (the library data/bss shifts and the sound block's own shift, see the
  lessons in `readme-translate-to-cpp.md`, are encoded once, here).
- Struct views for records as they are identified (text window, ship,
  projectile, campaign unit, IOAudio request, ...), each with its size
  asserted.
- A scratch allocator on the emulated stack (RAII) for rule 10.
- Hardware registers and anything with read/write side effects keep
  going through `Machine::read16/write16`; struct views are for RAM only.

Acceptance: one existing small routine rewritten with these types passes
the gate; no function body in `src/game` contains a numeric address.

### T5. Routine names and thunks

- `call68k(...)`: call an original (or L1) routine from L2 with typed
  arguments and result, without a per-call-site return offset. Use one
  fixed return trampoline instead of the caller's original return address
  (compiled C never inspects its return address; the gates will show if
  some routine does). Keep the existing exact mechanism for L1 code.
- A routine called from L2 gets a C++ declaration with a real name. While
  it is L0 or L1 its definition is a one-line thunk through `call68k`,
  kept together in one file per module so the remaining thunks are a
  visible to-do list. When the routine becomes L2, the thunk is deleted
  and the real function takes the name.
- `tools/analysis/progress.py`: per module, instructions at L0 / L1 / L2,
  printed by the gate. (The measurement used for section 2 is in the
  2026-11-12 session; it walks call targets in `build/dis/*.s` and the
  registrations.)

Acceptance: an L2 function calls an L0 routine, an L1 routine and an L2
routine by name; the gate passes; the progress line appears.

## 8. Pilot: the audio code

Convert `src/native/storm_sound.cpp` (about 1,100 lines, complete across
Storm, Title, RedStart and Red, self-contained, exercised constantly) to
L2 under `src/game/audio/`.

- This tests every rule in section 5 and every task in section 7 on real
  code. Where a rule turns out wrong or unclear, **fix this document** in
  the same commit.
- Then stop and show the user the result (section 11) before converting
  anything else. The pilot decides whether the style is what the user
  wants; changing the style after ten subsystems is expensive.

## 9. Order of work after the pilot

Work by subsystem, not by "whatever is ready". For each subsystem:
convert its L1 routines to L2, translate its remaining L0 routines
directly to L2, name its variables and records, add scenarios for what is
not covered. Then the next subsystem.

Suggested order (each builds vocabulary the next ones use):

1. Screen and text library shared by all modules (text window, cursor
   cell, print, fill, lines, blits, shapes, pictures, pointer, palette).
2. Input (keys, mouse, menu selection).
3. Red: strategic map, units, orders, the report and message screens.
4. Storm: ship and contact records, sonar and detection, weapons and
   projectiles, damage, the tactical displays, the battle loop. The
   largest remaining routines are here ($24684, $236a0, $2704c, $268ca,
   $20b8e: 540-970 instructions each); do them after the records they use
   have names.
5. RedStart: menus, setup, ship identification. Title: the sequence.

Do **not** spend effort on code that will be deleted in section 10:

- the RedStorm loader (loading and starting modules as processes),
- the insert-disk requesters and the `need_disk` copies,
- the C runtime (startup, exit, malloc, stdio, string functions),
- AmigaOS plumbing (message ports, opening devices and libraries).

Leave these at whatever level they are now. Existing L1 versions stay as
they are until they are deleted.

Routines that write hardware registers directly (Red $285e8, RedStart
$24bd0) are drawing primitives: translate them to L2 against
`Machine::write16` like any other code when their subsystem comes up.

## 10. After all game logic is L2

Do not start this section without the user (section 11). Outline only, so
that earlier work aims the right way:

1. **Freeze a reference.** From the last build that passes the RAM gate,
   record per-frame framebuffer hashes (`[fbhash]` exists) and add an audio
   hash, for every scenario. These recordings replace the RAM gate from
   here on.
2. **Replace the plumbing** listed in section 9 with host code: one
   program that calls the modules' entry functions directly, host file
   I/O and memory, a small audio and input layer.
3. **Move state out of the memory image.** Because of rule 2, this is a
   change to the declarations: the big-endian field types become native
   types, `Ptr<T>` becomes `T*`, named globals become members of state
   structs. Function bodies should barely change.
4. **Remove the interpreter and the AmigaOS layer**; keep or replace the
   planar display and blitter emulation behind the drawing functions.
5. Restructure freely (merge duplicated module code paths, rename, split
   files) with the frame and audio recordings as the gate.

### Step 2 plan (2026-11-14)

What still runs as 68000 code, in the order it gets replaced. After each
part tests/check_reference.sh must pass (and, while the interpreter
exists, tests/check_native.sh).

a. **The loader's game flow** (RedStorm $201b0 and the module runners
   $207f2/$20868/$208ca): the memory check, the preloads by free memory,
   the 164-byte shared record, the order Title, then RedStart / Red /
   Storm until the end. Becomes a host-side driver in C++ (`src/game/
   driver`), still starting each module through the HLE process calls,
   so the modules do not notice.
b. **Module start and exit** (each module's C startup, `title_start`-like
   routines, the hand-over of the view and the record): the driver calls
   the module's main routine directly with its a4 set, instead of a
   process with a WBStartup message. Process creation, ports and
   FindTask("RedStormRising") go away; the shared record becomes a
   struct the driver owns.
c. **The C runtime** (malloc/free, stdio, printf conversions, string
   functions, argument parsing, the random seed): host implementations
   with the same results in emulated memory (malloc must return the same
   addresses while state still lives there: keep the original allocator's
   layout, or move this after step 3).
d. **Disk requesters and need_disk**: the data is always present; they
   become no-ops (the reference never shows a requester).
e. **OS plumbing left** (input.device handler, pointer events, timer and
   audio device I/O): host input and audio layers feeding the same
   variables the L2 code reads.

### Step 3 plan (2026-11-14)

Game state moves out of the modules' small-data areas into C++ structs
with native types, one block of related variables per commit, starting
with Red's campaign state (no OS structure points into it).

- **Where it goes.** Per module a `State` struct (`src/game/red/state.hpp`
  for Red) and one object of it; bodies use the fields (`clock.minute`)
  instead of `*clock_minute`. Records keep their struct, with native
  field types.
- **Lifetime.** `GAME_STATE` (`src/game/mem/state.hpp`) registers the
  object: it is reset when the module starts (it replaces BSS, which the
  C start-up clears), and it is part of the RAM hash, so the determinism
  check covers it. Data-hunk variables (initialised, kept while a module
  stays loaded) come after the BSS.
- **Overlaps first.** The original reaches the same bytes under several
  names (`phase_words[8]` covers `war_phase_14394` .. `target_14408`;
  `loadout_flags` is a long whose bytes are flags of their own). A block
  moves as a whole, with all its names: `tools/analysis/statemap.py`
  lists every declaration's range, the extent the scenarios used, and
  which overlap, from an `RSR_TRACE=1` run.
- **Boundaries.** Where the state meets emulated memory (the player's
  record, which goes to Storm and to the save files; text buffers passed
  to the drawing code; addresses passed to routines that take `uint32_t`)
  the move converts explicitly: `to_image` / `from_image` for the record,
  routines taking `const Date&` instead of an address.
- **Checks.** Each module lists the a4 ranges that moved
  (`kMovedRanges`). The gate's first scenario run traces L2's memory
  accesses (`RSR_STATE_TRACE`) and fails on any access to a moved range.
  The reference recordings are the check on behaviour. A scenario's
  `set`/`get` of a moved variable reaches the native field
  (`GAME_STATE_FIELDS`).
- **What it costs.** A routine whose state moved reads the C++ state
  whoever calls it; the original 68000 copy (and `--native off`, and
  `RSR_NATIVE_SKIP`) works on the emulated memory, which is no longer
  kept up to date.
- **Reads outside a table.** The original sometimes indexes a table with
  -1 or past its end and gets the neighbouring bytes; a native array
  does not. Look for such indices (route ends, "none" values) when a
  table moves; the recordings catch them only on paths they take, the
  gate's bounds build catches them on those paths even when the picture
  does not change. Routines that now take a C++ reference are registered
  `GAME_L2_INSIDE` (no hook).

### Step 4 plan (2026-11-14)

What the C++ code still needs from the emulated Amiga (inventory at the
start): about 80 distinct OS calls through `game::os` (each loads 68000
registers and runs a library vector, which traps into the HLE), the
screen code's direct blitter register writes (`hw::`), the frame
timer and interrupts, the module loader (LoadSeg), and some 17 thunks into
original code that no scenario reaches (requesters, alerts, stdio write,
low-memory paths). Most of the HLE behind the OS calls is already C++
(src/amiga). The emulated memory itself stays for now as the game's
memory image (bitmaps, the modules' data, the state not yet moved);
removing it is step 5.

a. **A platform layer called directly.** `src/game/platform/`: the
   services the game uses, as plain C++ functions. Each OS call group is
   rewired from the 68000 trampoline to them, one group per commit:
   1. dos: files, delay (host file I/O into the memory image, same data
      and save directories);
   2. exec memory: AllocMem/FreeMem/AvailMem/CopyMem, AllocRaster (the
      machine's allocator directly; AvailMem's answers matter: the loader
      preloads by free memory);
   3. graphics drawing: Text, RectFill, Move/Draw, ReadPixel, BltBitMap,
      fonts (the HLE drawing code, called directly), and the screen
      code's blitter register writes as one host blit call;
   4. display: screen, window, palette, pointer, the shown bitmap; the
      frame composed from them natively instead of by copper/Denise;
   5. input: IDCMP messages and input.device as a host event queue;
   6. audio: audio.device requests to a host Paula mixer;
   7. timing: the vertical blank calls vblank_server from the host's
      frame loop; ports, signals, tasks and Forbid/Permit go.
b. **No CPU on the way in.** The driver calls the modules' main
   functions directly; "the running module" (today the a4 register)
   becomes a host variable.
c. **The original code left** (the thunks): each replaced by a small C++
   equivalent or removed with its unreachable caller.
d. **Modules as data.** The hunk files are read into the memory image by
   host code (relocated as LoadSeg does): their tables and texts are
   still used; no code runs from them.
e. **Remove** the interpreter, the HLE, the custom chip emulation and
   the per-call verification; the gate keeps the recordings, determinism,
   the trace and the bounds build.

The recordings stay the gate. The hard part is time: the frame a picture
or a sound appears depends on --det-timing's model (scanlines per OS
wait, CPU cycles of the 68000 trampolines). Each step must keep the
frame-by-frame recordings identical, so timing that the CPU provided
implicitly is made explicit where the step removes it (hw::os_wait_time
is the model).

### Item 3 plan: the memory image (2026-11-15)

User's order after step 4: (1) Storm's pointer events, (2) the shared UI
state, (3) the memory image, (4) the chip emulation, (5) restructuring.
For (3): `RSR_DATA_WRITES=FILE` lists the bytes of the modules' images a
run changes; over the 13 scenarios about 100 declared variables plus
some undeclared bytes still change in memory (2026-11-15). Blocks, one
or more commits each, the recordings and the moved-state trace as gate:

a. **Scalar state left in Var/PerModule/LibVar:** the audio library's
   (sound clock, enabled, channels, tick countdown, frame counter...),
   the C runtime's (allocator lists, stack bound, program name, WB
   message, current dir), the loader's (memory flags, module handles,
   mode), Storm's and Red's remaining scalars. As before: into the
   module's or the library's C++ state, aliases resolved by the trace.
b. **Text buffers and styles:** the drawing calls take host strings and
   C++ TextStyle values (the HLE drawing reads a host buffer instead of
   emulated memory); then the buffers (number_text, long_text,
   packed/unpacked text, window text...) and the styles become C++.
c. **Library structures:** the LZW decoder's dictionary, the animation
   objects and region lists, the pointer images (copied into the
   sprite at SetPointer).
d. **Records:** the player and campaign records and the module record
   between the loader and the modules: C++ structs, converted to bytes
   only where they go to a file (saves) — to_image/from_image.
e. **The OS plumbing left in memory:** the allocator's own lists, ports,
   IO requests, the WB start message, the task structure: replaced by
   host objects where the game only passes them along.
f. **Constant data** (texts, tables in the data hunks): read from the
   module files into host tables at load, no longer through the image.
g. What stays in emulated memory until item 4: chip data the custom
   chip emulation reads (bitplanes, BitMaps, palette/copper, fonts'
   glyph data, sound samples) and the OS display structures behind it.

### Item 4 plan: the chip emulation (2026-11-20)

User's choice after item 3: (4) replace the chip emulation. Today no
68000 code runs; the custom chips are driven by the HLE (Intuition and
graphics build a copper list from the screens' ViewPorts; Denise renders
it at the end of each frame), by the game's own blitter routines
(registers, screen/blitter.cpp, red/menu.cpp), by graphics' drawing
(amiga::Drawing) and by the audio device (Paula DMA). Steps, one or more
commits each, the recordings (frame and audio hashes) as the gate:

a. **Display:** the copper list and the display registers go; the HLE
   keeps the shown screens as typed bands (planes, depth, modes,
   position, colours) set at RethinkDisplay/LoadRGB4/SetRGB4, and the
   frame is composed from them (keeping the copper's timing: a band's
   colours from the line before it, colour 0 of the last band outside).
b. **Blitter:** the game's blits and graphics' drawing call a typed host
   blitter (the same minterm, mask, shift, fill and line logic) instead
   of writing registers; the register file goes.
c. **Audio:** the audio device feeds a host Paula mixer (the same
   per-scanline sample stepping) instead of the DMA registers.
d. **Ports:** the mouse counters, joystick and CIA fire bits become host
   input state the game reads.
e. **Chip data to host memory:** bitplanes, shapes, fonts and samples in
   host buffers named by handles, BitMaps and the OS display structures
   as typed host objects; then the emulated memory can go.
   The user chose (2026-11-20) to do 4e before restructuring, trimmed:
   e1. **Chip memory:** planes (screen, loader and animation bitmaps,
       the shape buffer and the temporary plane) and samples live in a
       chip region of the address space above RAM (0x800000-0xFFFFFF)
       whose bytes are host buffers; every reader and writer (display,
       blitter, graphics drawing, decoder, audio, file reads, copies)
       reaches them through Machine::bytes(addr, n). A bus access there
       is outside RAM: "[io]", the gate fails.
   e2. **Fonts:** the TextFonts from the font files as host objects.
   e3. **File data:** the preloaded files, the file buffer, the offset
       tables in host buffers.
   e4. **A native display API** in place of the Intuition/graphics
       emulation (the game uses one screen, one backdrop window, the
       pointer): its structures as typed host objects, not the
       Kickstart layouts.
   e5. **The rest:** the module images only as host copies, no emulated
       stack (Scratch) or heap, then no emulated RAM.

### Item 5 plan: restructuring (2026-11-21)

The game is C++ over typed state; what is left of the port's scaffolding
is what makes it hard to read. One step per commit, the recordings (NTSC
gate, PAL runs against the last build of item 4) unchanged:

a. **No dispatch by address.** The routines the game still calls through
   addresses (the effect table, the modules' start-up and main) become
   C++ function tables, with the time those calls took kept
   (Machine::call_timed); the running module becomes an explicit
   context. Then the 605 registrations by original address (kept as
   comments), the hook tables and the progress tools that measured the
   translation go.
b. **Names.** The fields and variables named by an offset (63) get names.
c. **Typed handles.** BitMaps, styles, menus, screens, records by C++
   reference instead of by emulated address.
d. **Constant data as typed tables** instead of declarations by a4 offset.
e. **A small native display, audio and input layer** in place of the
   Intuition/graphics/audio-device emulation.
f. **Merging the per-module copies** of the libraries where they are the
   same code.

### Item 5g plan: the memory image goes (2026-11-21)

The user accepted that the battles' terrain may change. The gate keeps
the recordings through --reference-terrain; a step that changes play
otherwise needs the user's visual check and new recordings
(tests/record_reference.sh), named in its commit.

0. The reference terrain (done).
1. The memory the original allocated only for the OS (process
   structures and stacks, start messages, module handles, library
   bases) and the heap reservations for host blocks go: memory is
   allocated where it is used.
2. File data, pictures and samples in host buffers (std::vector) instead
   of the emulated arena; the decoder and the sound code on host bytes.
3. State in native types (no big-endian fields, no a4 offsets for what
   moved to C++); scenarios name fields instead of offsets.
4. The constant data read from the program files into typed tables at
   load; module images and the emulated address space go.

## 11. When to stop and ask the user

- After the pilot (section 8): show before and after of a few functions.
- Before starting section 10.
- When a rule here cannot be followed for a whole class of routines.
- When a dependency must be installed, or a visual or audible check by a
  person is needed.

Otherwise keep working and committing without asking, as before. Keep the
"Status" section below current, and the memory files in step with it.

## 12. Commit checklist

- `bash tests/check_native.sh <scratch dir>` passes.
- New or converted routines are executed by a scenario (coverage report),
  or are on the unverified list with a reason.
- No register access, numeric address, or return offset in `src/game`.
- Every new name has a declaration comment.
- One subsystem step per commit; message says what moved to which level.

## Status

- 2026-11-12, `536912f`: strategy written. Nothing of section 7 is built
  yet. Next: T1.
- 2026-11-13: T1 done. `tools/analysis/liveness.py` writes
  `src/native/live_after_call.inc` for all 1,069 directly called routines
  (not only the exceptions: the verifier needs the d0 width too; the gate
  checks the file is current). Callers read the condition codes of 5
  routines for certain (the `mulu32` family and one more per module) and
  of 9 "maybe"; d1/a0/a1 of 30 (assembly helpers). `verify_native` now
  compares only what is in the table: flags when live or maybe (X only
  when live), d0 by parts, d1/a0/a1 of C routines only when live.
  `NATIVE_L2` registers a readable function; loading a module logs an
  error, which fails the gate, if callers read more than its declared
  result. Checked by injection in `clamp`: wrong flags pass, a wrong
  result is a MISMATCH. Next: T2.
- 2026-11-13: T2 done. Every routine is verified per call (all
  registrations now pass `true`; `false` is an opt-out nobody uses). The
  reference run throws `NotRepeatable` at the first OS call outside
  `Hle::repeatable` (graphics drawing, pens, palette, Init*, CopyMem,
  LockIBase and other no-ops), at a busy wait, or at process exit; the
  call is then counted as skipped and run natively. Under `--det-timing`
  WaitBlit and GetMsg let a scanline pass, so they are not repeatable
  there. Result: 119,694 calls verified (was 51,533) in 317 of 433
  functions (was 100), no mismatches; `verify-abort` lines in the log say
  which call ended the skipped reference runs (GetMsg, DoIO, audio BeginIO
  lead). Flags and d1/a0/a1 are compared only where the liveness tool
  found a read ("maybe" counts only for d0 within the declared result): 9
  routines have "maybe" flags, all through code the disassembly loses
  track of. Injection in `fill_rect` (one pixel narrower) is reported as
  a MISMATCH. Next: T3.
- 2026-11-13: T3 done. `RSR_COVERAGE` (and `RSR_COVER=1` for
  `run_scenarios.sh`) records executed original instructions;
  `tools/analysis/coverage.py` reports per module and per native routine;
  the gate prints it. Test scripts have `set`/`get` for module variables
  (a4-relative or absolute). New scenarios: `campaign_battle` (the
  submarine is put on the Soviet group's track: a real battle, replay,
  after-action report), `campaign_flags` (the same with flags for rarely
  reached branches), `battle_ice` (drift ice, depth mode 0, depth chart,
  pause with Alt-P, course order). Coverage of the original code: 52% ->
  56% (Storm 57% -> 66%); never-executed native functions 44 -> 18 (list
  below); per-call verify 260,073 calls in 332 of 433 functions, no
  mismatches. Game saves were not tried: `set` was enough. The gate now
  takes about 3 minutes. Next: T4.
- 2026-11-13: T4 done. `src/game/mem/types.hpp`: `be16`/`sbe16`/`be32`/
  `sbe32` fields (alignment 1, so a struct of them is the record's exact
  layout), `view<T>(addr)`, `Ptr<T>`, `address_of`. They map straight onto
  the emulated RAM array (RAM writes have no hooks in this machine).
  `src/game/mem/globals.hpp`: `Var<T>` (a4 offset), `Table<T>` (records),
  `LibVar<T>` (screen library, shifted per module), `PerModule<T>`, and the
  RAII `Scratch` on the emulated stack. Declarations per module in
  `src/game/<module>/vars.hpp` (Red: `UnitRecord`, `unit_records`). Proof:
  Red `unit_step` ($2c9fa) moved from `src/native/red_map.cpp` to
  `src/game/red/units.cpp` as L2 (`NATIVE_L2`), verified on 700+ calls.
  `tools/analysis/check_l2.py` (in the gate) rejects CPU state, raw memory
  access, register-level calls and 4+ digit literals in `src/game` bodies
  (`src/game/mem` and headers are exempt). Next: T5.
- 2026-11-13: T5 done. `src/game/mem/call.hpp`: `call68k<R>(address,
  args...)` pushes Aztec-style arguments and returns into the host
  trampoline (`call_sub` with no return address); the RAM-hash gate shows
  no difference from the original return addresses. Each module has
  `src/game/<module>/calls.hpp` (names of everything L2 code calls) and
  `thunks.cpp` (one line per routine not yet L2: the to-do list; exempt
  from `check_l2.py` because it holds the addresses). Red `scroll_input`
  ($227d4, the player's submarine following the map cursor) is L2 in
  `src/game/red/map.cpp`: it calls the L2 `unit_step` directly and the L1
  `scroll_speed`, `map_pixel`, `select_entry`, `detect`, `input_bits`
  through thunks; with `RSR_NATIVE_SKIP=red_scroll_speed,red_pixel,
  red_detect` the same thunks run original (L0) code and all scenarios
  still match `--native off`. New records: `MapPoint`, `MapUnit`.
  `tools/analysis/progress.py` prints L0/L1/L2 per module in the gate:
  L0 38%, L1 61%, L2 0.1% of 70,284 instructions (routine bodies between
  call targets; section 2 counted differently). Next: the audio pilot.
- 2026-11-13: audio pilot done (section 8). `src/native/storm_sound.cpp`
  (1,395 lines, all L1) is deleted; `src/game/audio/` has `vars.hpp`
  (IOAudio, Channel, Sample, EngineProgram, every variable named, 160
  lines), `audio.cpp` (the 36 routines in ~520 lines, one function for all
  four module copies), `calls.hpp` and `thunks.cpp` (6 routines outside
  the subsystem). Storm's `engine_update` and Red's `effect_none` moved to
  `src/game/storm/sound.cpp` and `src/game/red/units.cpp`; Storm's L1
  `clear_9222` is the L2 `engine_off`. New: `src/game/mem/os.*` (OS
  calls), `call68k` by run-time or per-module address, `Ptr<T>::at<U>`
  and indexing, `NATIVE_L2_ALSO`. Things learnt while naming: the
  per-channel "lengths" are the io_Unit channel masks, and the L1 names
  of the engine program's period and volume words were swapped (the
  program is: sample, volume, one note). Gate: equal RAM hashes in all
  seven scenarios after the two verification changes in section 6
  (indirect-only routines, process stacks); per-call verify gives up at
  audio.device BeginIO (not repeatable), so the player is checked mainly
  by the whole-program comparison, which covers every path of
  sound_tick except the loop-2 repeat and the two clamps. Levels: L0 38%,
  L1 51%, L2 9.9%. Next: show the user (section 11) before converting
  anything else.
- 2026-11-13: the user approved the style; section 9 starts. Step 1,
  part 1 (screen library: display and drawing) is L2 in
  `src/game/screen/` (`vars.hpp`: BitMap, RastPort, RasInfo, ColorMap,
  Screen, TextStyle and the library variables; `display.cpp`: which
  bitmap is drawn into and shown, palette, colour registers, pointer;
  `draw.cpp`: pens, lines, plots, rectangles, boxes, blits, masked
  shapes, Storm's images). 44 routines in up to five modules replace L1
  code in `storm_screen.cpp`, `storm_misc.cpp`, `storm_text.cpp` and
  `red_ui.cpp`; the d7-saving wrappers and the copies at other addresses
  are registered on the same functions. Red's `text_screen` became L2
  too (its L1 version passed on set_palette's leftover d0, which per-call
  verify compared), and Red's map code calls `screen::draw_to` instead
  of a thunk. New: `GAME_L2`, graphics and intuition calls in
  `src/game/mem/os.*` (which now preserve d2-d7/a2-a6 like the C
  bindings), `ptr_to`, `string_length`. The fill_rect "target not in the
  table" case is kept (draws as bitmap -1). Gate: equal in all seven
  scenarios; 238,853 calls verified in 319 of 428 functions. Levels: L0
  38%, L1 46%, L2 14.2%. Next: part 2, text (draw_text, widths, the text
  helpers) and loading shapes and pictures.
- 2026-11-13: step 1, part 2: text and loading. `src/game/screen/text.cpp`
  (draw_text, text_at, text_str, text_lines, char_width and its
  wrappers), `pictures.cpp` (load_shape with its mask, load_picture,
  Storm/Title show_screen; disk or preloaded copy), and Storm's printing
  helpers and status line in `src/game/storm/text.cpp` (text,
  text_at_cell, text_shadow, text_jam2, draw_char, text_on_bitmap1/2,
  status_message, status_bearing, status_if_flag, clear_status). String
  operations on emulated memory are in `src/game/mem/types.hpp`. Two
  findings: Storm's "style word 0" is the text style's bitmap number, so
  text_style1/2 print onto bitmaps 1/2; draw_text's result is read as a
  long by some callers (its high word is what Text leaves, 0), so it is
  declared `int32_t`. Draw modes other than JAM1/JAM2 (the original would
  pass on its caller's d5) are not used and are drawn as JAM1. Gate:
  equal in all seven scenarios; 232,198 calls verified in 317 of 427
  functions. Levels: L0 38%, L1 43%, L2 17.8%.
- 2026-11-13: step 1, part 3: the hand-written blitter routines and the
  animated objects. `src/native/blitter.cpp` is deleted:
  `src/game/screen/blitter.cpp` (copy_region, draw_shape: the blitter
  programmed through the named registers of `src/game/mem/hardware.hpp`,
  with OwnBlitter/DisownBlitter) and `animation.cpp` (AnimObject,
  AnimCommand, RegionList; flush_regions, draw_object, draw_objects and
  the script interpreter animate) for Storm and Title. Title's run
  executes all of draw_shape; not executed anywhere: the script's S
  command, an I shape of -1, and copy_region/draw_shape clipping at the
  top edge in Storm. Gate: equal in all seven scenarios; 226,747 calls
  verified in 315 of 427 functions. Levels: L0 38%, L1 38%, L2 23.0%.
  Next: step 2, input (keys, mouse, menu selection).
- 2026-11-13: step 2 done: input is L2 in `src/game/input/`
  (`input.cpp`: getkey, flush_keys and its wrappers, read_stick,
  key_down from the keyboard matrix, controls; `menu.cpp`: RedStart's and
  Red's menu_key, menu_controls with mouse steering, menu_move).
  `src/native/storm_input.cpp` is deleted; `menu.cpp` keeps only the
  frame ticks (they busy-wait: Red/RedStart step). New OS calls (GetMsg,
  ReplyMsg, DoIO, LockIBase/UnlockIBase) and port registers (JOY1DAT,
  CIA-A PRA). One deviation: with mouse steering on and the menu (right)
  button down, the original starts the bits from its caller's d7; L2
  starts from 0 (no scenario does this). Steering towards the upper left
  is not executed by any scenario. Gate: equal in all seven scenarios;
  191,089 calls verified in 314 of 424 functions. Levels: L0 38%, L1
  35%, L2 26.2%. Next: step 3, Red.
- 2026-11-13: step 3, Red, part 1: unit movement on the strategic map is
  L2 in `src/game/red/movement.cpp` (map_distance and its wrappers,
  passed, unit_heading, set_velocity, move_unit, set_heading_bits,
  set_speed, order_start/order_next, init_headings, nearest_of,
  unit_frame, set_ranges, scroll_speed; the jump stubs to them are
  registered on the same functions); `src/native/red_map.cpp` is deleted.
  UnitRecord, MapUnit and Place are named field by field: +$20/+$22 are
  the y/x velocity, bits 0/1 of +2 mark moving left/up, the L1 "speeds"
  of set_speeds are the units' detection ranges, and scroll_speed picks
  the player's speed (5 still, 15, 30 fast) with matching ranges. The
  saturating abs is L2 in `src/game/common/math.cpp` (its L1 version
  stays for storm_string.cpp's direct calls). Gate: equal in all seven
  scenarios; 176,128 calls verified in 308 of 424 functions. Levels: L0
  38%, L1 33%, L2 27.8%.
- 2026-11-13: Red part 2: the per-frame unit steps are L2 in
  `src/game/red/frame.cpp` (map_frame, detect, in_range, the steps of
  units 2, 3, 5, 6, 7, the tick-3/5 enemy steps, the pointer unit, and
  the submarine's base/target/meeting tests). MapUnit +8 is a mode word,
  UnitRecord +0 a state (-16 idle, -1 arrived), unit 9 follows the mouse
  pointer. Levels: L0 38%, L1 32%, L2 28.4%.
- 2026-11-13: Red part 3: the front lines are L2 in
  `src/game/red/fronts.cpp`. What the L1 code called "groups" and
  "slots" are 8 fronts (90 bytes at $304e0) with 4 walkers each that
  redraw a front's segments pixel by pixel in the advancing side's
  colour; stages 0-6 move a front's end. Also map_pixel, plot_all,
  recolour, and the jsr/rts wrappers of flush_keys and controls.
  Levels: L0 38%, L1 31%, L2 29.4%.
- 2026-11-13: Red part 4: routes are L2 in `src/game/red/routes.cpp`
  (at_sea, patrol, pick_bit, next_target, next_place, unit3_destination,
  unit6_route, wander, order). UnitRecord +8 is the place a unit comes
  from, +10 the next place on its route, +12 the route's end, +28/+30 a
  patrol's position; Place +0 is its kind, +6 the places it leads to.
  Coverage: unit6_route, next_target, patrol and wander fully executed,
  next_place and unit3_destination nearly. Levels: L0 38%, L1 30%, L2
  30.5%.
- 2026-11-13: Red part 5: the war's phases and battle groups are L2 in
  `src/game/red/campaign.cpp` (phase_values, count_class, enough_units,
  score_factor, new_phase, update_targets, group_speed, unit_anim,
  group_event, group_state, group_wait, input_bits, mission_setup).
  Findings: a record's +$14 is its maximum speed, the slowest member's
  (L1: "strength"); input_bits sets the submarine's velocity; MapUnit +8
  doubles as the frame of a hit animation. Levels: L0 38%, L1 29%, L2
  31.4%.
- 2026-11-13: Red part 6: the campaign clock and small helpers are L2 in
  `src/game/red/clock.cpp` (advance_time with the reinforcements,
  run_minutes, set_message, order_code, put_char, mark_redraw,
  reinforce_chance, pick_mission_char); the random number generator
  (random_n, rnd, Storm's rnd_inclusive) is L2 for all modules in
  `src/game/common/random.cpp` (the L1 random_n stays for direct
  register-exact callers). Levels: L0 38%, L1 29%, L2 32.1%.
- 2026-11-13: Red part 7: `src/game/red/groups.cpp` (form_group,
  start_group, draw_symbol, draw_units). MapUnit +2 is its symbol
  character, +18 whether it is drawn in bitmap 0/1, +20 an offset from
  its position where it is drawn; the units' symbols use a text style at
  a4+52 whose offsets are the map view. Levels: L0 38%, L1 28%, L2
  32.7%.
- 2026-11-13: Red part 8: `src/game/red/turn.cpp` (turn, map_screen,
  mission_names); `red_frame.cpp` keeps only memset and skip_strings (C
  runtime). Levels: L0 38%, L1 28%, L2 33.0%.
- 2026-11-13: new scenario `campaign_base` (the submarine set on its
  base, place 11 at (40, 120)): the base report path of Red's turn; 570
  more Red instructions executed (Red 61% -> 64%, all 57%). Still not
  executed: the turn's meeting with the target (needs the submarine's
  state a4+14402 = $5x with a4+14504 >= 10).
- 2026-11-13: Red part 9: the text page is L2 in `src/game/red/text.cpp`
  (text_object, flush_text, record_char, text_char, cell_char,
  two_digits, string_width, skip_strings, clear_text_page, print_text
  with its control codes, clear_line_rest, clear_cells, fill_style,
  print_name); the waits of RedStart and Red in
  `src/game/input/wait.cpp` (wait_released, wait_key, wait_seconds) and
  read_palette in `display.cpp`. TextStyle is complete: +2/+4 origin,
  +6/+8 size, +18/+20 cursor, +22 margin. Levels: L0 38%, L1 27%, L2
  33.8%.
- 2026-11-13: Red part 10: hex_byte, number_cells, print_hhmm,
  print_date (a DateTime record: year in BCD, month, day, hour, minute),
  print_status_word, clear_to_end, clear_row_rest join
  `src/game/red/text.cpp`. Levels: L0 38%, L1 26%, L2 34.3%.
- 2026-11-13: Red part 11: the loadout screens are L2 in
  `src/game/red/loadout.cpp` (print_name_count, load_row, count_and_name,
  two_counts, loading_panel, weapon_list, tube_rows, tubes_screen,
  tube_status, tube_status_rows, gauge_row, status_line, print_header,
  mission_summary). Reading the strings behind the a4 offsets (with
  loadseg from the module file) showed what the L1 "status" and
  "orders" screens are: the weapons on board with load times ("add",
  "remove", " min"), torpedo tubes 1-8, "operational"/"inoperative", the
  war day and a PACT/NATO gauge. Levels: L0 38%, L1 25%, L2 35.7%.
- 2026-11-13: Red part 12: `src/game/red/pages.cpp` (show_text_page for
  RedStart and Red, show_message, message_page, map_picture,
  enter_map_screen: the L1 "picture screen" is the strategic map,
  picture 2). Levels: L0 38%, L1 25%, L2 36.2%.
- 2026-11-13: Red part 13: day_message, briefing_message and the
  campaign page ("TOP SECRET", date, "Cmdr ... USN", "USS ...", the day
  and mission names) join `pages.cpp`. Levels: L0 38%, L1 24%, L2
  36.6%.
- 2026-11-13: Red part 14: the control room page (L1 briefing_page:
  sonar contact with its bearing, launch window, or no contacts; the
  menu; navigation) and count_terrain join `pages.cpp`. Levels: L0 38%,
  L1 24%, L2 36.9%.
- 2026-11-13: Red part 15: the ship status page (L1 debriefing_page),
  news_report and end_mission join `pages.cpp`. Levels: L0 38%, L1 23%,
  L2 37.3%.
- 2026-11-13: Red part 16: campaign_end, promotion (the result message,
  the next rank bit and new name into the player's record) and
  award_medal join `pages.cpp`. Levels: L0 38%, L1 23%, L2 37.7%.
- 2026-11-13: Red part 17: `src/game/red/menu.cpp` (highlight, choose:
  a text page's two-column menu by mouse or keys; draw_icons: the medals
  held). The L1 "message" variables a4+15632.. are the current menu
  (keys, index, side, list; set_menu). check_l2.py now also catches
  numeric literals with a u/l suffix. Levels: L0 38%, L1 23%, L2 38.2%.
- 2026-11-13: Red part 18: loadout_screen (loading the tubes: a4+14516
  is the tube count, the tubes' loads follow), draw_entry and add_row
  (RedStart and Red), and the controls wrapper. Levels: L0 38%, L1 22%,
  L2 38.5%.
- 2026-11-13: Red part 19: menu_page (blocks of text lines and
  selectable rows) and window_char for RedStart and Red join `menu.cpp`.
  `red_ui.cpp` keeps alert (plumbing) and two RedStart routines. Levels:
  L0 38%, L1 21%, L2 39.6%.
- 2026-11-13: Red's original (L0) routines start: load_campaign ($20f40,
  436 instructions, 408 executed by the scenarios) is translated
  straight to L2 in `src/game/red/record.cpp`, with the PlayerRecord
  layout (496 bytes: career, names, rank bits, the saved campaign).
  Levels: L0 38%, L1 21%, L2 40.2%.
- 2026-11-13: career_page ($237aa, the after-action/career page with the
  ship tally and medals; at the campaign's end the final rank) from the
  original to `pages.cpp`. Levels: L0 37%, L1 21%, L2 40.7%.
- 2026-11-13: save_campaign ($2dd10: the campaign's state into the player
  record before a battle, the inverse of load_campaign) from the
  original to `record.cpp`. Levels: L0 37%, L1 21%, L2 41.0%.
- 2026-11-13: pick_mission ($21c1c: home, 9/10 by the reserves, or a
  weighted pick among the 70 missions whose rules (MissionRule,
  MissionRange in Red's data) allow them) and must_go_home ($21e48)
  from the original to `campaign.cpp`. Levels: L0 37%, L1 21%, L2
  41.4%.
- 2026-11-13: ship_picture ($28274: a ship's picture by name from the
  OSG file into the library's shape bitmap, drawn at (166, 0)) from the
  original; the screen library's load_shape now shares build_shape and
  read_file_part with it. Levels: L0 36%, L1 21%, L2 41.6%.
- 2026-11-13: single_mission ($2189c: a single mission from an order:
  the enemy group, terrain odds, headings, date, campaign page) from the
  original to `campaign.cpp`. Levels: L0 36%, L1 21%, L2 41.8%.
- 2026-11-13: port_screen ($2ac1c: at the base, the tubes emptied into
  the stock, weapons loaded/unloaded with their load times, tube
  systems repaired) and next_repair ($2af04) from the original to
  `loadout.cpp`. Levels: L0 36%, L1 21%, L2 42.1%.
- 2026-11-13: new_campaign ($20ddc), choose_missions ($21aec: the two
  battle groups' missions, avoiding recent ones and needing the units)
  and recently_played ($21bd0) from the original. The L1
  "pick_mission_char" is the second group's mission choice
  (pick_second_mission). Levels: L0 35%, L1 21%, L2 42.5%.
- 2026-11-13: init_map_units ($20b22), battle_result ($2168e), start_red
  ($20ce0) and red_main ($20210, Red's main loop: turns frame by frame
  until a battle, saving the campaign for Storm) from the original. One
  deviation: red_main's result starts at 0 where the original starts
  from its caller's d7; every path sets it before it is used except a
  campaign that ends in the first frame. Levels: L0 35%, L1 21%, L2
  42.9%.
- 2026-11-13: enemy_step ($245b6), group_hunt ($24696), pursue
  ($22702) and unit7_move ($22b78) from the original. Place 68 is where
  the submarine was last sighted. Levels: L0 35%, L1 21%, L2 43.2%.
- 2026-11-13: tally_battle ($22d40), remove_losses ($22e7a), conroom
  ($2a9f8: the control room loop), reset_route_units ($20c12),
  picture_for ($23ef2: the twelve page pictures "CAP", "CONROOM",
  "MAP", ...), turn_check ($20642), restore_positions ($2184e) and
  base_report ($2abec) from the original. Levels: L0 34%, L1 21%, L2
  43.7%.
- 2026-11-13: recolour_rect ($285e8, the blitter recolouring of a
  rectangle: three blits find the pixels of a colour in a temporary
  plane, then the differing planes are flipped through it) from the
  original, through the named custom chip registers; the verifier no
  longer compares a6 of L2 routines (section 6). Levels: L0 34%, L1 21%,
  L2 43.9%.
- 2026-11-13: sheltered ($2c2ee: three areas where the submarine is not
  pursued) and after_tally ($22ef2: "Enemy Damage Assessment", then the
  promotion) from the original. Red's executed game code is now all L2;
  what remains L0/L1 in Red is the LZW decoder, the C runtime, OS
  plumbing (module start, the timer interrupt, the screen to front, the
  random seed) and routines no scenario executes (the end of a single
  mission, $2146c, $2bbd8, $2353c, $235e8 and others). Levels: L0 34%,
  L1 21%, L2 44.0%. Next: step 4, Storm.
- 2026-11-13: step 4, Storm, part 1: `src/game/storm/battle.cpp`
  (in_map_area, mark, update_ready, handle_keys, enter_mode3/5,
  log_event, log_ship_event, select_next_contact) and reset_regions (the
  L1 init_views) in the screen library. Ship (62 bytes at $3a2d2),
  Object (48 at $3a606) and LogEntry (6 at $3a8d0) are started. One
  deviation: mark draws unknown kinds in colour 0 (the original uses its
  caller's d4; only kinds 0-3 occur). Levels: L0 34%, L1 21%, L2
  44.6%.
- 2026-11-13: Storm part 2: depth_at, refresh_status(2), wait_frames,
  start_message, random_sea, report_events, show_mode, gauge,
  change_speed, change_rudder, change_scale join `battle.cpp`;
  cycle_colours (the script's C command: colour cycling) joins the
  screen library's animation. Ship +26/+28 are the ordered speed and
  course; ShipType (34 bytes at $35064) started. Levels: L0 34%, L1
  20%, L2 45.5%.
- 2026-11-13: Storm part 3: load_tube, flash_red/black, enter_mode3b,
  track_ship (the detection score), order_depth (the L1 "order_speed":
  the strings and rudder_step show Ship +24 is the ordered depth, +12
  the depth), order_course. Levels: L0 34%, L1 19%, L2 45.9%.
- 2026-11-13: Storm part 4 (src/game/storm/ships.cpp): keep_highest,
  trim_step (Ship +16 is the heading), ship_step, reload_tick; sign and
  clamp join common/math for Storm, RedStart and Red. Levels: L0 34%,
  L1 19%, L2 46.3%.
- 2026-11-13: Storm part 5: unload_tube (the L1 "fire_tube": the
  string is "UNABLE TO UNLOAD TUBE"), draw_ticks, approach,
  steer_torpedo (select_button: left/right), move_projectiles
  (Projectile, 26 bytes); sin_mul/cos_mul join common/math. Object +16
  heading, +28 course, +40-44 the wire. Levels: L0 34%, L1 18%,
  L2 47.3%.
- 2026-11-13: Storm part 6: map_view (the 6-byte "waypoints" are the
  battle log; display 10 replays it), weapon_step (a4-31112 is
  "aiming", formerly status_off_a), rudder_step, depth_limits (running
  aground / hitting the ice). Levels: L0 34%, L1 17%, L2 47.9%.
- 2026-11-13: Storm part 7: battle_step (one step of the simulation),
  roll_highest, remove_object (the L1 reset_entry48). Ship +0 is the
  side; Object +42 is its time, +44 the time below which it seeks. Its
  step routines (trim_step, rudder_step, ship_step, depth_limits,
  approach, ...) are now called directly, so the per-call verify lists
  them as not reached; they are checked as part of battle_step. Levels:
  L0 34%, L1 17%, L2 48.3%.
- 2026-11-13: Storm part 8: log_all (a full log starts again: log_end,
  log_wrapped), random_conditions, random_damage (own_damage, formerly
  player_lost_31173: bits 0-3 systems, 5 iceberg, 6 aground, 7 sunk),
  sea_shades. Levels: L0 34%, L1 16%, L2 48.7%.
- 2026-11-13: Storm part 9: sea_noise, depth_chart, launch (noisemaker,
  decoy, knuckle), hit_ship (ShipType +32 is its toughness). Levels: L0
  34%, L1 16%, L2 49.3%.
- 2026-11-13: Storm part 10: next_contact, object_step (Object +12 is
  the depth, +14/+26 speed and set speed), order_key and input_number
  (a4-30862 is the number entry's mode, formerly status_off_b). Levels:
  L0 34%, L1 15%, L2 50.1%.
- 2026-11-13: Storm part 11: fire_weapon (Object +38 is the weapon
  type, formerly "attack"; +30 noise, +36 tube). Levels: L0 34%, L1
  15%, L2 50.6%.
- 2026-11-13: Storm part 12: enemy_launch (ShipType +31 its weapons,
  Ship +60 when it last fired). Levels: L0 34%, L1 14%, L2
  51.0%.
- 2026-11-13: Storm part 13: detect (sonar contacts; Ship +7 bits 0-1
  how well it is identified, +42/+44 and +52/+54 the two sonar range
  and signal pairs; ShipType +0 its name). Levels: L0 34%, L1
  14%, L2 51.5%.
- 2026-11-13: Storm part 14: attack_score (ShipType +6/+12/+18 the
  per-weapon rating, arc and guidance, +24 defence), below_limit;
  log_scale joins common/math. Levels: L0 34%, L1 13%, L2
  51.9%.
- 2026-11-13: Storm part 15: aim_cursor (AimPoint, 12 bytes at
  a4-32700), save_aim ($2a668, original), attack_check (sonobuoys:
  projectile kind 5, which attack_score reads as a Ship). The last of
  src/native/storm_battle.cpp; the file is gone. Levels: L0 34%, L1
  13%, L2 52.5%.
- 2026-11-13: Storm part 16 (src/game/storm/panels.cpp): threats_label,
  weapons_label, helm_labels, clear_lower_panel, draw_letter_list (the
  names are in pen 0 on pen 9: the original's reused argument slot puts
  9 in the background pen), entry_line (the L1 "weapon_line": the number
  entry's question and digits), reloads_list. Levels: L0 34%, L1
  12%, L2 52.8%.
- 2026-11-13: Storm part 17: draw_grid, tactical_background,
  shade_column (with sea_shades now sharing ShadeLevels), torpedo_labels
  (the L1 labels_291fa). One deviation: draw_grid's pen on displays
  other than 3 and 5 is a local the original never sets; it was 6 in
  every scenario (display 10, the battle log's map), so L2 uses 6.
  Levels: L0 34%, L1 12%, L2 53.2%.
- 2026-11-13: Storm part 18: tubes_list (with the TASM count),
  own_ship_panel (the L1 ship_panel; Ship flags bit 0: cavitating),
  damage_list, left_panel, right_panel (their page routines $21808,
  $2198c, $2953e, $243ce are thunks). Levels: L0 34%, L1 11%,
  L2 53.8%.
- 2026-11-13: Storm part 19: draw_display (displays 1/3/5, 2, 4, 6, 8
  are thunks; 7 is sea_noise), redraw, letters_display,
  ship_list_display (the L1 ship_list). Levels: L0 34%, L1 11%,
  L2 54.0%.
- 2026-11-13: Storm part 20: torpedo_panel (the threats panel: Threat,
  6 bytes at a4-31224), helm_panel (Ship flags: 0 cavitating, 1 towed
  array, 2 mast, 4 noisy, 5 active sonar). Debugging tip: when an L2
  routine is called directly by other L2 code, RSR_NATIVE_SKIP does not
  reach it; route the call through call68k(address) for the bisect.
  Levels: L0 34%, L1 11%, L2 54.6%.
- 2026-11-13: Storm part 21: sonar_display (display 4: the strings show
  a4-31098 is the thermal layer's depth, now layer_depth; a4-31092 and
  a4-31090 the surface duct's and the layer's strength; a4-31096 the
  ambient noise), set_display, sonar_library_display ($22bac) and
  sonar_library ($22c02). src/native/storm_text.cpp is done and gone.
  Levels: L0 34%, L1 10%, L2 55.1%.
- 2026-11-13: Storm part 22: bearing and course_to join common/math
  (Storm, RedStart, Red), set_tone joins audio, area_level (the sea bed
  map in the code hunk), save_battle_state (BattleRecord at *a4+5234),
  fresh_screen. Left in storm_misc/math/string: C runtime, loader and OS
  plumbing (section 9: not translated). Levels: L0 34%, L1 9%,
  L2 56.1%.
- 2026-11-13: Storm part 23 (original routines): replay_log ($2d408,
  the battle log replay; $2d658 is a thunk), replay_battle ($2d3c6),
  enter_display5, tubes_page, aim_to_screen, load_aim. Also fixed in the
  HLE (not the game code): BltMaskBitMapRastPort painted black outside
  the mask (seen on the periscope and sea conditions displays; native
  off and on alike, so the gate could not see it). Levels: L0 33%,
  L1 9%, L2 56.5%.
- 2026-11-13: Storm part 24 (original routines): redraw_screen
  ($2dda2), show_event ($2dd4c; a4-31178 turns the event pictures on,
  formerly "sound_off"), rbu_attack ($28b50), weapon_page ($29172).
  Unverified: the scenarios run with event pictures off, so
  redraw_screen and the picture path are not reached; no scenario has an
  RBU attack. Levels: L0 33%, L1 9%, L2 56.7%.
- 2026-11-13: Storm part 25: periscope_display ($21e5c, display 6).
  Levels: L0 33%, L1 9%, L2 56.9%.
- 2026-11-13: Storm part 26: periscope_view ($22020: ships on the
  surface by bearing and aspect, the waves, the reticle). New scenario
  battle_periscope (9 scenarios now): sea conditions, the periscope too
  deep and at 50 feet. Levels: L0 33%, L1 9%, L2 57.3%.
- 2026-11-13: Storm part 27: weapon_panel ($2953e; Object +46 is
  homing/jammed/searching), start_aiming ($297da). Levels: L0 32%, L1
  9%, L2 57.9%.
- 2026-11-13: Storm part 28: tactical_analysis ($2ce2c, the Help key:
  the message for keep_highest's top situation). battle_periscope now
  presses Help (raw $5F, the Home key on the host). Levels: L0 32%,
  L1 9%, L2 58.1%.
- 2026-11-13: Storm part 29: display_setup ($2b6cc, formerly the thunk
  display_scale_2b6cc; its jump table: 2 $2a818, 4 sonar_display, 6
  periscope_display, 7 sonar_library_display, 8 letters_display),
  contact_display ($2ab30, display 2: Ship +36-40 and +46-50 are its
  gauges). Levels: L0 31%, L1 9%, L2 58.8%.
- 2026-11-13: Storm part 30: sonar_analysis_display ($2a818, display
  2's frame), after_action_report ($2c776). Levels: L0 30%, L1
  9%, L2 59.6%.
- 2026-11-13: Storm part 31: sonar_update ($22766, the sea conditions
  picture each frame; covered by battle_periscope). Levels: L0 30%,
  L1 9%, L2 60.1%.
- 2026-11-13: Storm part 32: contact_label_panel ($21808). $2dea4 (the
  event pictures' file loading) is file plumbing and stays L0. Levels:
  L0 30%, L1 9%, L2 60.1%.
- 2026-11-13: Storm part 33: contact_panel ($2198c). Ship +42/+52 are
  sensor indices (sensor_names at a4-31312), not ranges: heard_by and
  hears_by. Levels: L0 29%, L1 9%, L2 60.7%.
- 2026-11-13: Storm part 34: replay_step ($2d658, one moment of the
  battle log's replay). Levels: L0 29%, L1 9%, L2 61.2%.
- 2026-11-13: Storm part 35: general_quarters_report ($2c292, the
  report before a battle). Levels: L0 28%, L1 9%, L2 61.8%.
- 2026-11-13: Storm part 36: move_ship ($260cc; Ship +56 the towed
  array's heading, +58 the heading 128 frames ago). The original's
  sea-bed check for enemy ships discards its result (no effect); kept
  so. Levels: L0 27%, L1 9%, L2 62.5%.
- 2026-11-13: Storm part 37: hear ($27c74: the three sensors' scores,
  the signals and the advice levels 6-10). Levels: L0 27%, L1 9%,
  L2 63.2%.
- 2026-11-13: Storm part 38: ship_tactics ($28552, the enemy's
  behaviour). Levels: L0 26%, L1 9%, L2 63.9%.
- 2026-11-13: Storm part 39: enemy_weapon_step ($2781c, formerly the
  thunk "decoy_step": Object kind 3 are the enemy's weapons; +32/+34
  their bearing and range). Levels: L0 26%, L1 9%, L2 64.4%.
- 2026-11-13: Storm part 40: type_data_page ($2afce, a ship type's data
  base page). battle_periscope now also opens the identification list
  (F7) and Shift+A. Levels: L0 25%, L1 9%, L2 65.1%.
- 2026-11-13: Storm part 41: object_seek ($268ca: a seeking weapon,
  homing and hits). Levels: L0 24%, L1 9%, L2 66.0%.
- 2026-11-13: Storm part 42: player_weapon_step ($2704c, formerly the
  thunk "contact_step"; ShipType +26 its air defence). Levels: L0
  23%, L1 9%, L2 67.0%.
- 2026-11-13: Storm part 43: tactical_display ($236b2, displays 1/3/5
  each frame; Ship/Object +18-22 the last drawn screen position and
  heading). The objects' stored heading comes from a local only the ship
  loop sets (the last ship's), kept so. With enter_display1
  ($236a0, in front of it). Levels: L0 22%, L1 9%, L2 68.3%.
- 2026-11-13: Storm part 44: setup_ship_list ($20aaa), setup_ships
  ($20b8e: the start of a battle; BattleStart +70 enemy count, +72 their
  types, +92/+94 the submarine's speed and heading). Levels: L0 21%,
  L1 9%, L2 69.2%.
- 2026-11-13: Storm part 45: battle_init ($20774). bytes_fill joins
  mem/types.hpp. Levels: L0 20%, L1 9%, L2 69.5%.
- 2026-11-13: Storm part 46: battle_main ($20228, Storm's main loop;
  BattleStart +2/+4/+6/+8 the level, settings and class). Levels: L0
  20%, L1 9%, L2 70.1%.
- 2026-11-13: Storm part 47 (src/game/storm/keys.cpp): process_key
  ($24684, every battle key). The strings named more flags: a4-30886
  paused (formerly status_flag_30886), a4-30868 sounds on, a4-31178 the
  action track (event pictures); Ship flags bit 2 is the active radar.
  Levels: L0 18%, L1 9%, L2 71.5%.
- 2026-11-13: Step 5 (RedStart) begins: choose_row (RedStart $2463e,
  Red $29a2e, shared library code: the row menus' chooser; RowMenu +4-8
  the grid's end and column step). Shared copies are found by diffing
  the modules' disassembly (pairs of a4 offsets give the PerModule
  values). Levels: L0 18%, L1 9%, L2 71.9%.
- 2026-11-13: end_mission (RedStart $21408) and draw_icons ($263b4)
  shared with Red: campaign_record, medals_held, medal_masks and
  medal_shapes are PerModule. Levels: L0 18%, L1 9%, L2 71.9%.
- 2026-11-13: RedStart part 1 (src/game/redstart/saves.cpp): load_file,
  save_line, save_game, recall_game, medal_row, view_honor_roll,
  add_to_honor_roll (SaveEntry 80 bytes, FameEntry 40; os::write and
  os::delay added). New scenario redstart_menus (10 scenarios): RECALL,
  EXIT, View HONOR ROLL. Unverified: save_game, save_line, medal_row and
  add_to_honor_roll need a game in progress or saved files (the
  scenarios start with an empty RedStormSave:). Levels: L0 17%, L1
  9%, L2 72.8%.
- 2026-11-13: RedStart part 2 (menus.cpp): main_menu ($203cc, the first
  menu), identification_test ($20d72, the warship identification test).
  PlayerRecord +0 is `started`. Levels: L0 17%, L1 9%, L2
  73.1%.
- 2026-11-13: RedStart part 3: choose_rank ($20ee2), show_assignment
  ($21076), choose_difficulty ($21154), choose_scenario ($211e0).
  mem::code_address added for the code pointers the original stores (a
  row hook RedStart never calls). Levels: L0 16%, L1 9%, L2
  73.6%.
- 2026-11-13: RedStart part 4: choose_era ($20d1c), enter_name
  ($20e64), wrong_answer ($20ece, formerly welcome), intro_sequence ($2128c), intro_page
  ($212be), store_choices ($21330), page_sequence ($24424: with its flag
  only keys below space end the sequence). Next in RedStart: the main
  routine $201f8 and entry $201b8, input_text $24a02, the shared
  routines $24bd0 (Red's recolour_rect), $26676/$26d98 (page_picture,
  ship_picture), $25d6c, $25f2e, $264d8; then Title. Levels: L0 16%,
  L1 9%, L2 73.9%.
- 2026-11-14: RedStart: input_text ($24a02, the name typed; Red's copy
  $29df2 is never called) and start_main ($201f8, RedStart's main
  routine: the menus in order). The manual protection: the emulator NOPs
  the original's branch after identification_test ($202dc); start_main
  asks `mem::protection_bypassed()` instead, so --protection still works.
  string_copy_n (strncpy) joins mem/types.hpp. The entry $201b8 stays
  original (process start-up and exit). Levels: L0 16%, L1 9%, L2
  74.1%.
- 2026-11-14: RedStart's copies of Red's L2 recolour_rect ($24bd0),
  picture_for ($26676) and ship_picture ($26d98) registered; their
  tables, the temporary plane and the ship file's variables are
  PerModule (the page_picture and special_page thunks are gone).
  Levels: L0 15%, L1 9%, L2 74.7%.
- 2026-11-14: $25d6c, $25f2e and $264d8 (RedStart) are not translated:
  copies of Red library code that nothing in RedStart reaches (no call
  chain from the start-up or from a code pointer; $264d8, a copy of
  show_message, even has an unresolved `jsr 0(pc)` in place of
  print_text). The tools took the "dos.library" string after every
  module's start-up for a `bsr.s $20220`, splitting the main routines
  (start_main, red_main) in two: `liveness.DATA_IN_CODE` drops it
  (live_after_call.inc regenerated). Levels: L0 15%, L1 9%, L2 74.8%.
- 2026-11-14: RedStart L1 to L2: draw_spans ($20388; Span records) and
  select_and_store ($249c8), now red::draw_to_window (Red's copy $29db8
  is never called). Levels: L0 15%, L1 9%, L2 74.9%.
- 2026-11-14: The packed texts (src/game/red/texts.cpp, Red and
  RedStart) from L1 to L2: unpack_text, copy_unpack, message, message2,
  text_n, text_n_ptr. mem::PerModuleData names data the original
  addresses absolutely (the message tables). All callers are L2 now, so
  the hooks are not reached; routed through call68k for one run,
  unpack_text (827 calls) and text_n (782) verified without mismatches.
  Levels: L0 15%, L1 8%, L2 76.2%.
- 2026-11-14: Step 5 Title begins with the animation sequences shared
  with Storm (src/game/screen/sequences.cpp): play_sequence (Storm $2de04,
  formerly the thunk play_event; Title $2040c), start_sequence ($2e416 /
  $20a1e), load_anim_script ($2e2a0 / $208a8), load_anim_shapes ($2dea4
  / $204ac), free_anim_shapes, free_anim_script, anim_error. This was
  Storm's "event-picture file loading". Storm reads the preloaded copies,
  Title the files, so both paths run; the error paths do not. New:
  os::alloc_raster/free_raster, hw::wait_until (a busy wait on the frame
  timer: lets scanlines pass, as the deterministic spin check does for
  the original), screen thunks alert/c_malloc/c_free; input::frame_timer
  is a LibVar (data 9704). Levels: L0 13%, L1 8%, L2 77.9%.
- 2026-11-14: Title (src/game/title): title_main ($201b0, the module's
  main routine) and opening_picture ($23f20, "RedStormA:OPENING", always
  from disk). Thunks: title_start ($20228) and title_exit ($20278), the
  process start-up and the hand-over to the next module (loader
  plumbing, section 9), need_disk_a ($23c90, RedStormA:). What is left in
  Title is the C runtime, the insert-disk requester $23d1c, lzw_code
  ($24092, also RedStart, Red: the pictures' decompression) and $2453a /
  $24410 (around it). Levels: L0 13%, L1 8%, L2 78.1%.
- 2026-11-14: The library's text helpers (src/game/common/strings.cpp)
  from L1 to L2: format_number (Storm's thunk "level_name" was this),
  number_field, find_name (Red's thunk "find_ship"), concat. itoa and
  ltoa_pow stay L1 behind common::format_int / format_long. Levels: L0
  13%, L1 7%, L2 78.3%.
- 2026-11-14: free_block (audio, all four modules) and the frame tick
  (src/game/input/tick.cpp: frame_tick, RedStart $269e4 / Red $288e0,
  and Red's frame_tick_wait $2891a) from L1 to L2; src/native/menu.cpp
  is gone. The tick's busy wait uses hw::wait_until. Levels: L0 13%, L1
  7%, L2 78.7%.
- 2026-11-14: The picture decoder (src/game/screen/decoder.cpp, all five
  modules) to L2: unpack, unpack_file, picture_palette (formerly the L1
  copy_saved_block), and inside them the LZW codes, the run-length rows
  and the bitplane writers; src/native/lzw.cpp and fill_tables are gone.
  Its state is the Decoder record (an absolute block per module, kept
  byte for byte, the stored code pointers included). unpack verified per
  call (10 RedStorm calls). New marker GAME_L2_INSIDE: original routines
  done inside an L2 function (no hook); registrations.py counts them as
  L2. unpack_file: no scenario. Levels: L0 12%, L1 5%, L2 82.4%.
- 2026-11-14: Scenarios campaign_group0 (the battle counts as group 0's:
  battle_won) and single_mission_end (the end of a single mission:
  single_mission_end, the final outcome page $235e8, back to RedStart);
  gate 12 scenarios. The scripts' `set` takes *a4+N+M (a variable behind
  a module's pointer): both change the record's situation through
  Storm's record pointer during the battle.
- 2026-11-14: Red's endings to L2 (pages.cpp): outcome_page ($235e8, the
  final outcome page; formerly "not identified"), single_mission_end
  ($21a3e), end_phase ($2353c, the war's end: not reached). The outcome
  record (6 bytes at a4+14610) and its tables are named. Levels: L0 12%,
  L1 5%, L2 82.7%.
- 2026-11-14: battle_won ($2146c, battle group 0's battle judged by the
  mission type) with half_lost ($2151c/$21532) and targets_sunk ($21548)
  to L2 (turn.cpp); campaign_group0 runs the half_lost branch. Levels:
  L0 12%, L1 5%, L2 82.9%.
- 2026-11-14: hunt_check ($2bbd8, a hunting group heads for the place
  nearest the sighting) to L2 (campaign.cpp); not reached. Levels: L0
  11%, L1 5%, L2 83.1%.
- 2026-11-14: The last L1 game helpers to L2: format_degrees,
  format_thousands and copy_static (common/strings.cpp), random_step (no
  caller left; random_n steps inline), flag_names, set_row_hook and
  set_text_list (RedStart, formerly set_pointer_var_a/b; red::row_hook is
  PerModule). Levels: L0 11%, L1 4%, L2 83.3%.
- 2026-11-14: All game logic is L2. The original code still executed is
  the C runtime (startup, argument parsing, malloc, stdio, number
  conversions, the random seed from the date), the RedStorm loader and
  the modules' start-up/exit hand-over, the insert-disk requesters, and
  OS plumbing (input.device, the pointer, ports). What stays L1 is the
  same kind of code. Section 10 needs the user (section 11).
- 2026-11-14: The user started section 10. Step 1 done: `--av-hash N`
  logs digests of the picture and the sound of each N-frame block;
  tests/record_reference.sh recorded every scenario (--det-timing,
  60-frame blocks, NTSC) into tests/reference (384 KB);
  tests/check_reference.sh compares. The original code (--native off)
  gives the same recordings in all 12 scenarios, so they stand for the
  original game.
- 2026-11-14: Step 2a: RedStorm's game flow to L2 (src/game/redstorm):
  redstorm_main ($201b0), the module runners, the preloads, the shared
  record (ModuleRecord, 164 bytes), the display and library set-up and
  tear-down, the offset tables. opening_picture is shared by RedStorm and
  Title (screen/pictures.cpp). Still original (thunks): the C runtime and
  the module processes (load/start/wait/unload: step 2b). Both gates
  pass. Levels: L0 10%, L1 4%, L2 85.2%.
- 2026-11-14: Step 2b, first part: the modules run inside the loader's
  process (redstorm::run_in_loader): their WBStartup message goes to the
  loader's own port, its pr_CLI is hidden meanwhile, and the module's
  start-up code is called on a stack allocated as CreateProc did (with
  the process structure and name it kept). Storm reads a local it never
  set, so the module's stack contents must be the original's (a cleared
  stack at the same place): with the loader's stack the contact
  signal differs in battle_ice. hw::os_wait_time() stands for the
  loader's WaitPort (a scanline under --det-timing). No processes are
  created any more. The RAM comparison with --native off ends here (the
  original loader lays out memory differently); tests/check_native.sh
  now checks determinism and the reference recordings instead. The
  per-call verify counts fell during the day (63336 calls this morning,
  6013 now) because callers became L2 and call directly: the recordings
  cover those calls.
- 2026-11-14: Step 2d: the disk checks (need_disk_a / need_disk, RedStormA:
  and RedStormB:, in every module) are empty L2 functions; the insert-disk
  requesters are no longer reachable. Reference unchanged. Levels: L0
  10%, L1 4%, L2 85.5%.
- 2026-11-14: Step 2c begins: the C runtime's memory allocator
  (src/game/common/memory.cpp: c_malloc, c_free, lmalloc, add_free,
  alloc_block, free_blocks; Storm, RedStorm, Title, and free_blocks in
  RedStart and Red) to L2, keeping its memory layout (blocks from AllocMem,
  a free list sorted by address, the deferred free). The L1 aztec_malloc
  and pool_free are gone. Levels: L0 9%, L1 4%, L2 86.6%.
- 2026-11-14: The game modules' C start-up and exit to L2
  (common/startup.cpp): module_startup at $20008 (the module's a4 and BSS,
  dos.library, the Workbench message through the real WaitPort/GetMsg,
  main(0, message) as _main does with the empty command line) and
  program_exit at $20154, which does the exit work and unwinds to the
  start-up with mem::ProgramExit (Machine::call_sub now restores its state
  when an exception passes). Red, RedStart and Storm end through exit();
  Title returns from main. RedStorm keeps its own start-up (a CLI start,
  its exit ends the program). Levels: L0 8%, L1 4%, L2 86.8%.
- 2026-11-14: The C runtime's exit through stdio to L2 (common/stdio.cpp,
  all five modules): exit() (buffered files written out; the write stays
  a thunk), _exit (open handles closed), the close and the Ctrl-C check.
  With native code on, the scenarios now run about 900 distinct original
  instructions: the module hand-over (entries, start/exit, save_view),
  the device plumbing (input.device, pointer, VBL ticker), the random
  seed from the date, RedStorm's own start-up. Levels: L0 7%, L1 4%, L2
  88.0%.
- 2026-11-14: The vertical-blank ticker to L2 (audio.cpp: vblank_server,
  start_ticker in Storm, Title, RedStart, Red): the frame counter, the
  music's tick, the window's mouse position, PAL's extra tick.
  GAME_L2_SERVER registers an interrupt server (d0 and the flags from the
  result: exec runs the next server only after Z).
- 2026-11-14: The module hand-over to L2 (common/module.cpp): load_view,
  save_view, copy_colours (from L1), module_start and module_exit (Title's
  former thunks, Storm's, and RedStart's/Red's end_mission), and the
  modules' main routines storm_entry, redstart_entry, red_entry. The
  shared record (ModuleRecord) is in common/record.hpp. Levels: L0 7%,
  L1 3%, L2 88.8%.
- 2026-11-14: Device plumbing to L2 (common/devices.cpp): the message
  ports (create_port, delete_port, from L1, all modules), the I/O requests,
  the system time from timer.device and the random seed taken from it
  (Storm, RedStart, Red), Red's ScreenToFront/ActivateWindow. Levels: L0
  6%, L1 3%, L2 89.7%.
- 2026-11-14: Storm's input.device plumbing (storm/pointer.cpp: open,
  close, move_pointer, which writes one input event) and RedStart's
  save_disk_ok to L2 (the requester stays original, never reached). With
  native code on, the game modules now run no original code in the
  scenarios; what remains is RedStorm's own start-up, _main and
  unload_module. Levels: L0 6%, L1 3%, L2 89.9%.
- 2026-11-14: RedStorm's own start-up (the CLI path of module_startup),
  its exit and unload_module to L2. Milestone: with native code on, the 12
  scenarios now interpret no original instruction at all (coverage files
  of a --native on run are empty). What original code is left is not
  reached by them (requesters, error paths, the low-memory paths, the
  stdio write); the L1 routines still run (number conversions, compiler
  arithmetic helpers, string functions, RedStorm's module handles).
  Levels: L0 6%, L1 3%, L2 90.1%.
- 2026-11-14: The last L1 routines the scenarios call to L2: itoa and
  ltoa_pow (common/strings.cpp), Red's Delay wrapper ($2f184, formerly
  "exec_198": it calls dos Delay), RedStorm's module handles (new_handle,
  free_handle, load_module, path_join); the unused start_module and
  wait_module are gone. Levels: L0 6%, L1 2%, L2 90.7%.

- 2026-11-14: Section 10 step 3 begins (plan above): the state registry
  (mem/state.hpp: reset at the module's start, in the RAM hash, scripts'
  set/get), the trace of L2's memory accesses (RSR_STATE_TRACE,
  tools/analysis/statemap.py) and the gate's moved-state check. First
  block: Red's campaign clock (red/state.hpp: `Date clock`), with the
  record's copy as a DateImage; print_hhmm and print_date take a Date.
- 2026-11-14: Red's zero block (a4+15554, never written: the original
  clears records by copying from it) is gone; L2 clears directly.
- 2026-11-14: Red's phase block (a4+14394, 50 bytes: the war phase, the
  missions, the order phase, targets, history, the campaign page's time,
  the detection masks) is `Phase phase` in red/state.hpp. mem/image.hpp
  writes and reads such structs as the record's big-endian bytes
  (IMAGE_FIELDS lists the fields once). The input tick shared with
  RedStart reads Red's two masks through PerModuleMixed (RedStart's stay
  in emulated memory).
- 2026-11-14: Red's battle groups (a4+14444, `Group groups[2]`) and the
  war block (a4+14616, `War war`: the situation, headings, terrain
  counts and the battle's group, a copy of one of the two) in C++.
- 2026-11-14: Red's loadout block (a4+14492, `Loadout loadout`: weapons,
  capacity, speed, tubes, the loadout flags and the damage bits, whose
  bytes the original also read on their own) in C++. A script's byte
  poke into the flags reaches their top byte (Field::width).
- 2026-11-14: Red's day timers and reinforcements (a4+14542, `Days`),
  the missions fought/won/lost (a4+14574, `Battles`) and the scores
  block (a4+14580, `Scores`: score, reserves, day count, class counts,
  medals, the ending's Outcome) in C++. The medal row shared with
  RedStart reaches Red's medals through PerModuleMixed.
- 2026-11-14: Red's mission block (a4+14656, `MissionState mission`: the
  order, the menu's request (also the input library's), the fronts kept
  between screens, three positions) and the units left (a4+16062) in C++.
  With it, every block of the player's record that Red saves and loads
  is native.
- 2026-11-14: Red's career fields (a4+16044: rank, rank step, rank bits,
  level, spread; `Player player`) and units_available (a4+16094) in C++.
- 2026-11-14: Red's battle tally (a4+15646, `Tally`: the enemy group's
  values before and after, sunk, hit, score, multiplier) and the turn's
  flags (a4+15500..15547 except the words the input library shares;
  `Turn turn_flags`) in C++.
- 2026-11-14: Red's map units (a4+14740) and unit records (a4+15104) are
  native structs in Red's state, with their aliases (sub_pos, enemy_start,
  player_vx/vy, scroll_target, unit8_place); map_distance takes two
  Positions and keeps its results in `Distance distance` (a4+15618).
  MapPoint (still the emulated places, fronts and cursor) converts to and
  from Position. The input library's steering reference reaches Red's
  submarine through PerModuleMixed.
- 2026-11-14: Red's initialised data that the game changes (the fronts,
  the places, the text page, the missions' eligible flags) in C++:
  GAME_STATE_DATA state is taken from the loaded image at the module's
  first start after a load (the loader reports loads) and kept while the
  module stays loaded. Two lessons: the original reaches into these
  tables under other names (unit 3's "35/39 may be chosen" words are
  fronts 2 and 3's `last`), and it reads outside them (next_target reads
  place -1's kind, the byte before the table), which a native array does
  not reproduce; the trace window now reaches a4-65536 so the gate sees
  Red's data below a4-32768.
- 2026-11-14: The gate gets an array-bounds step: a second build with
  `-fsanitize=bounds -fsanitize-trap=bounds` (build-bounds/) runs every
  scenario and fails if one stops. It found two more places where the
  original goes past a table: the text page cursor (a column of 2940
  writes Red's data after the page: page_byte sends such bytes to the
  emulated memory) and pick_mission's at_sea(place | 0x80), which reads
  "places" 128 and more (place_pos reads them from the emulated memory).
- 2026-11-14: Storm's ships, ship list, projectiles, objects and battle
  log (a4-8610..+5206, src/game/storm/state.hpp) in C++. attack_score
  takes the head the original reads of a ship, object or projectile
  (`Body`, built with the original's offsets). sound_flags_8606 and
  engine_speed_8596 were ship 0's flags and speed. A byte-wise write to
  a big-endian word (the log entry's event bits through its first byte)
  became bit operations: such reinterpret_casts are wrong on native
  fields.
- 2026-11-14: Storm's battle variables in its initialised data (a4-31248
  ..-30570: settings, controls, weapons and tubes, sea conditions,
  displays, counters, the log's indices; 84 of them) are `Battle battle`
  (GAME_STATE_DATA). Found by comparing RAM dumps of a battle
  (RSR_RAM_DUMP) and a static count of writes. Aliases: own_damage and
  action_track_low are low bytes of damage_word and action_track,
  steer_bits of steer_keys, tasm_count is weapon_counts[5], scale_mode3
  scales_by_mode[3]; the input library's no_return_fire is Storm's
  entry_mode (PerModuleMixed). The read-only tables between them stay.
- 2026-11-14: Storm's aiming, launch point, course, periscope and sea
  words (BSS a4-7118..-7078 and three more) are `Tactical sight`. The
  shared course_to writes Storm's course through PerModuleMixed.
- 2026-11-14: Storm's aim points (8, one per player weapon object),
  threats (6) and sonar shade pens in C++. The trace's moved-state check
  caught a first guess of 14 aim points running into the texts after
  the table.
- 2026-11-14: Step 4 begins (plan above). First: game::os calls no longer
  run through the 68000. Machine::call_trap runs the HLE for a library
  vector directly and charges the trap's, the RTS's and the return
  stub's 4 cycles each at the same points (with line boundaries in
  between), so real-time play keeps its timing; under --det-timing CPU
  cycles never end a line. Recordings unchanged, and the scenarios' last
  pictures without --det-timing are the same as before.
- 2026-11-14: Step 4a.1: the dos files are a host service (src/host/files:
  Open/Close/Read/Write/Seek/Lock and the save directory's copy on write,
  moved out of the HLE; the unused ADF path dropped). game::os's file
  calls use it directly; the HLE's dos part is register glue over it for
  any 68000 caller. tests/check_save_load.sh (broken since PAL became
  the default: its frames assumed real-time timing) runs with
  --det-timing again and is part of the gate.
- 2026-11-14: Step 4a.2: AllocMem, FreeMem, AvailMem, CopyMem,
  AllocRaster and FreeRaster call the machine's allocator directly.
- 2026-11-14: Step 4a.3: graphics.library's drawing (Text, TextLength,
  SetFont, SetRast, Move, Draw, RectFill, Read/WritePixel, the pens and
  draw mode, GetRGB4, BltBitMap, BltMaskBitMapRastPort) is the typed
  amiga::Drawing (gfx_draw.hpp); game::os calls it directly, the HLE's
  graphics vectors are glue over it.
- 2026-11-14: Step 4a.4 (the OS side of the display): open/close screen
  and window, ScreenToFront, MakeScreen/RethinkDisplay, SetPointer,
  ActivateWindow, LoadRGB4, SetRGB4, InitBitMap are typed Hle functions
  called directly; ShowTitle, (Un)LockIBase, the Workbench and the
  blitter ownership are no-ops in game::os. The view still becomes a
  copper list that the display emulation renders: section 10's step 4
  allows keeping the planar display and blitter emulation behind the
  drawing functions.
- 2026-11-14: Step 4a.5 begins: the window's input. The HLE keeps IDCMP
  events in a host queue (no IntuiMessages in emulated memory); getkey
  reads them through game::platform::next_input. Machine::os_call
  charges a replaced OS call's time (the det-timing scanline of a
  waiting call, the trampoline's cycles), so the recordings and the
  real-time scenario endings stay the same.
- 2026-11-14: Step 4a.5 continued: key_down reads the keyboard matrix
  through game::platform::read_key_matrix (no keyboard.device request).
  Storm's pointer moves go to game::platform::write_input_event; Storm
  no longer opens input.device (no event, port or request). Known gap,
  kept from the emulator: its input.device never carried these events
  out, so the pointer does not follow the aiming as on an Amiga; the
  recordings pin that (to do, with a visual check). Real-time
  (non-det-timing) runs drift a little from here (battle_ice's last
  picture): they were never a reference; the recordings are.
- 2026-11-14: The system time (the random seed) comes from
  game::platform::system_time instead of timer.device.
- 2026-11-14: Step 4a.6: audio.device's requests are host objects in the
  HLE (by key: an IOAudio address for 68000 callers, the game's own keys
  otherwise). The sound player sets period/volume, flushes, writes and
  checks through game::platform::audio_*; open_audio/close_audio allocate
  nothing (no requests, ports or device). The Paula model still plays.
- 2026-11-14: Step 4a.7 begins: the vertical-blank ticker is a host
  server (game::platform::add_vblank_server): the interrupt dispatch
  calls vblank_server directly, in the module context captured when it
  was installed (Machine::with_native_context), instead of through
  call68k and the hook.
- 2026-11-14: Delay and WaitTOF are host waits (game::platform::delay,
  wait_frame: the machine's clock and frame counter).
- 2026-11-14: Hooked routines are entered without the CPU: call_sub of a
  hooked address (module start-up, main, the exit and break handlers)
  invokes the hook directly (Machine::call_native), charging the return
  stub's cycles as before. The per-call verify left the gate (step 4e
  early): since 4a.6 the originals it runs as references find no audio
  requests (the game no longer builds them) and ran on garbage. Its
  mismatches had gone unnoticed for two commits: with pipefail,
  "grep | head -20 | grep ." in the gate was false when head stopped
  early; the gate's checks now collect into files first.
- 2026-11-14: Measured: with native code on, a --det-timing scenario run
  steps the 68000 through no instruction at all (boot, module start-up
  and main, interrupt servers, OS calls are all dispatched to C++
  directly). The CPU object still holds a4 (the running module's data)
  and a7 (Scratch locals on the emulated stack), and execute() is still
  the trampoline that enters RedStorm's start-up. Left of step 4: the
  exec/dos plumbing that remains (ports, tasks, libraries, LoadSeg,
  the WB start message: 4a.7), the thunks into original code (4c), the
  modules as data (4d), then removing the interpreter (4e).
- **Step 4a.8 (2026-11-15): exec and dos plumbing typed.** Tasks
  (FindTask), signals, message ports (PutMsg/GetMsg/ReplyMsg/WaitPort),
  OpenLibrary, OpenDevice/CloseDevice, CurrentDir and LoadSeg/UnLoadSeg
  are amiga::Hle typed functions (the exec/dos vectors are glue over
  them); game::os calls them through os_call (the same time). Forbid,
  Permit, AddPort, RemPort, CloseLibrary only take their time. Only
  begin_io and display_alert (original code no scenario reaches) still
  go through a library vector.
- **Step 4c part 1 (2026-11-15).** alert and fatal_error L2
  (screen/errors.cpp; the L1 alert deleted; fatal_error decides loader
  or module by the running module, see its comment), write()'s path
  L2 (fd_entry, dos_write, dos_seek, c_lseek, c_write in
  common/stdio.cpp), the bad-shape message by format_int instead of
  sprintf, thunks to routines already L2 (exit, c_exit, course_to)
  replaced by direct calls; DisplayAlert typed.
- **Step 4c part 2 (2026-11-15): no thunks left.** RedStart's
  save-disk requester is the library's disk requester (all five modules,
  `disk_requester` in screen/requesters.cpp); need_disk_a/need_disk
  (no-ops before) lock the volume and ask for the disk as the original.
  Script command `disks out|in` (tests: bare volumes cannot be locked);
  scenario `disk_requesters` (gate: 13 scenarios), its recording equal
  to the previous commit's run of the original requester code. Found:
  since 4a.5 `--native off` gets no IDCMP input (GetMsg on the window's
  port is empty: events go to the host queue), so original code there
  sees no clicks or IDCMP keys; compare input paths with an earlier
  commit's build (a worktree) instead.
- **Step 4e.1 (2026-11-15): no way back into 68000 code.** The code
  addresses the original stores (exit and break handlers, effects, a
  module's start, main) are run by `mem::call_routine`, which only enters
  native routines (a fatal error otherwise); os::begin_io and the
  register-loading `call` in game::os are gone. 4d needs nothing more:
  LoadSeg is host code and the modules' code is only data.
- **Step 4e.2 (2026-11-15): the original code is not run any more.**
  `--native off|verify`, the L1 routines (src/native keeps only the
  runtime: registry, running module, argument adapter), the liveness
  table, coverage, profile and instruction trace are gone; the CPU only
  enters hooks and the HLE's stub traps, and stops the program
  ("original code reached") at any module instruction (gate check).
  Gate steps for return addresses, liveness and coverage removed.
  Still the CPU object: its registers (a4, a7, d0) and cycle counter, the
  HLE's line-A glue and interrupt dispatch.
- **Step 4e.3 (2026-11-15): no interpreter.** Machine::run enters the
  launcher's entry routine natively and ends the process after it;
  src/m68k keeps only the register state (a4, a7, d0, the stack
  operations) and `cycles` (the timeline); cpu.cpp, ops.hpp, the
  execute loop, Machine::call68k and line-A dispatch are gone. Interrupt
  handlers/servers that are not host functions, and Cause, now stop the
  program (none is installed).
- **Step 4e.4 (2026-11-15): no library vectors.** The HLE's per-library
  dispatch (exec/dos/graphics/intuition/devices glue over the typed
  functions), device IO requests (BeginIO, keyboard/gameport/timer
  pumps, the IOAudio path), process switching (CreateProc, block,
  schedule) and SetIntVector handlers are gone. Boot still builds the
  libraries, the launcher process and their structures in memory (the
  game reads fields of them; the jump tables stay as data).
- **Step 4e.5 (2026-11-15): the CPU object is a register file.**
  m68k::Cpu keeps d0-d7/a0-a7 (the game uses a4 for the running module,
  a7 for the stack its arguments and scratch buffers live on, d0 for
  results), the hook address being entered, the interrupt mask, the
  stop flag, the stack operations and `cycles` (the timeline). Step 4 is
  done: the game is C++ over host services, with the emulated memory as
  its memory image and the custom chips (Denise/copper display, blitter,
  Paula) still emulated behind the drawing and sound functions.
- **Storm's pointer events (2026-11-15, user checked visually).** The
  input events Storm writes (IECLASS_RAWMOUSE relative, IECLASS_POINTERPOS
  absolute, in Intuition's high-res units) now move the pointer
  (Hle::pointer_event): after "4" the aiming crosshair starts on the
  aim point, and the wire-guided torpedo's steering point is kept. Six
  scenarios' aiming mouse moves were re-aimed to the same click points
  (script command `pointer` logs the pointer) and their references
  re-recorded; they differ only during aiming, sound unchanged.
- **Shared UI state, part 1 (2026-11-15).** `game::ui::State`
  (src/game/ui/state.hpp): the screen/input library's state in C++, one
  copy per module (`ui::state()` is the running module's), registered
  for all five modules with each module's shifted a4 ranges
  (`kMovedBss`, Storm's offsets + native::lib_shift_of). First block:
  drawing_to, showing, palette_changed, current_shape, select_button,
  last_key. Next blocks: the Data-block variables (initialised: window
  mouse, button_event, pointer_image; GAME_STATE_DATA-like loading per
  module), menus and steering (PerModule ones in RedStart/Red), frame
  ticks, animation; palette and bitmaps are passed by address to the
  drawing code (boundaries).
- **Shared UI state, part 2 (2026-11-15).** ui::State now lists each
  field's a4 offset per module (`kFields`, kAbsent where a module has
  none) and each module's real BSS (`kBss`: bss_start/end minus a4): a
  field that was BSS there is cleared at the start, one that was
  initialised data is taken from the loaded image and kept while the
  module stays loaded. (Part 1 had reset all of its fields: the
  library's "Bss" block is initialised data in every module, not the C
  start-up's BSS; LibVar's Block::Bss only names the library's block.)
  Moved: key169_flag, the window mouse position (= the menus'
  mouse_pos, Storm's click_x/y), the menu cursor/flags/dirs/key, mouse
  steering, space and help requests, menu toggle, last mouse, steering
  steps, the frame tick (double buffer, flip countdown/bitmap, frame
  ticked, flips, tick frames, last tick). Red keeps its names as
  accessors (map_active(), map_controls(), map_redrawn(),
  map_draw_bitmap(), map_ticks(), map_count() (bit 3 blinks),
  map_cursor(), ...). no_return_fire in Red is a4+0, its key169 flag
  (ui::states[4]). Finding aliases: an RSR_TRACE run lists STATE MOVED
  with the declaration's label (a pointer access has the raw label);
  a temporary abort there under gdb gives the accessor.
- **Shared UI state, part 3 (2026-11-15).** The library's handles,
  buffers and in-memory files moved to ui::State with their big-endian
  types (copied raw from the image; `FieldAt::big`): graphics,
  intuition, diskfont and dos bases, bitmaps[3], rastport, screen,
  window, fonts[4], planes buffer, keyboard port/request, shape memory,
  file buffer, frame timer, button event, the preloaded files and their
  offset tables and flags, pointer image, pointer_kept (was word_9738).
  Names the modules gave the same bytes, now resolved: the record's
  +128 is the shape memory (record field renamed); Red's mouse_clicked,
  temp_plane (planes buffer), entry_bitmaps; Red's disk_in/ship_offsets/
  preloaded_ships are the OSG (ship picture) file's; RedStart's
  disk_ok_16768 is shapes_preloaded; Storm's clicked (button_event),
  tick_counter (frame_timer), pointer_kept_9738. shape_mask stays in
  memory: it is shape_bitmap's eighth plane, and the blitter reads that
  BitMap from memory. Left in memory: palette and bitmaps' BitMaps
  (passed to the drawing code by address), shape_bitmap, the key
  matrix buffer, constant texts and tables (item 3), the animation
  state (Storm, Title), Red's and RedStart's text pages and styles.
- **Shared UI state, part 4 (2026-11-15).** Red's and RedStart's menu
  and text-page variables: menu keys, index, side, list, window style,
  row colours, page top, row hook, text list, page row/col, page shown
  (= Red's text_page_shown), page flags (15526 = Red's screen_flag;
  16128), the current page text style (= Red's text_style). The
  TextStyle structs themselves stay in memory (passed by address).
- **Item 3a, audio (2026-11-15).** mem/module_state.hpp: the
  per-module field mechanism (FieldAt tables, kBss, MODULE_STATE), now
  used by ui::State and the new audio::State (the sound code's channels,
  samples, clock, engine speed/running, quiet mode, preload block/
  offsets/flag, device-open and enabled flags, ticker bytes; RedStorm
  has the preload fields). Stays in memory: the requests, the engine
  programs and set_tone's two words (sound programs a channel plays by
  address), the frame counter (its address escapes). A file read into a
  moved struct goes through a Scratch buffer (load_sample's length).
- **Item 3a, the C runtime and the hand-over (2026-11-15).**
  common::State: random seed, the allocator's block list, first block,
  free bytes, deferred free, exec base copy, WB message, current dir,
  stack bound, program name, tool window, exit routine, fd count, break
  handler, dos error, errno, module/player record, start mode. The
  record's "code" fields are the pointer images (8x8, 7x7, none) and
  Storm's image list: named so (record fields, RedStorm's constants,
  ui::State). Accessors give each module's typed view of the player
  record pointer (Red's career(), RedStart's player(), Storm's
  battle_start()/battle_record()). Stays in memory: free_list (its
  address is the list's first link). Scripts: module_state fields are
  reachable by set/get at their old offsets (big-endian fields as in
  memory), and `*a4+N+M` takes the pointer from the native field (two
  scenarios set the record through Storm's a4+5234; a stale pointer in
  memory made them diverge late, not a game change).
- **Item 3a, RedStorm and RedStart (2026-11-15).** redstorm::State (the
  memory flags, workbench flag, mode, record, player, module handles)
  and redstart::State (current save/fame, start mode (+ byte), new
  player, era, rank row/code, difficulty, scenario), both on
  mem/module_state. Stay in memory: RedStart's text_pen_15636 (the fg of
  the text style at a4-15648) and intro_picture_29056 (inside 12 bytes
  at a4-29058), both reached by address. RSR_DATA_WRITES after this:
  left are the decoder's dictionary, animation state, text buffers and
  styles, sound programs, palette/BitMaps (3b, 3c, g).
- **Item 3b so far (2026-11-15).** Text goes to the drawing code as host
  strings: amiga::Drawing and os::text take const char*; the screen
  library's draw_text/text_at/text_str/text_lines take a TextStyle& and
  a host string; Storm's and Red's text functions take const char* with
  uint32_t overloads that forward emulated strings (mem::host_text);
  string_copy/append/equal accept host sources; the text helpers
  (format_number, number_field, format_degrees, format_thousands,
  concat, copy_static, flag_names) return const char*, and their
  buffers are char arrays in common::State. Functions whose signatures
  became host types are registered GAME_L2_INSIDE (no hook; only C++
  calls them). Left for 3b: the modules' own text buffers (Storm's
  text_buffer, last_status, hit_message, typed_digits; Red's pending,
  packed/unpacked text, symbol text, names; RedStart's window text and
  names) and the TextStyles (their pointers in ui::State become C++
  pointers: Ptr<T> to T*, section 10 step 3).
- **Item 3, after 3b/3c (2026-11-15).** Also moved: Storm's text_buffer,
  last_status, hit_message, typed_digits (storm::Texts); Red's/RedStart's
  packed/unpacked text, window/symbol text, pending text (an index now),
  row list, chosen row, window cursor, input positions (ui::State); Red's
  mission_line and news_front (red::Texts, its BSS hunk); the picture
  decoder (a native Decoder per module, no host pointers so it hashes
  deterministically); the animation player (screen::Anim). Kept in
  memory, with the reason: Red's number_text (lines of 64 bytes run over
  news_day, so the two share bytes). RSR_DATA_WRITES now shows per
  module only: palettes (library and module, passed by address to
  LoadRGB4), the TextStyles (styles are passed and stored by address:
  ui's window_style/row_colours/page_text_style, style_addr()), the
  player/commander/sub names (records, 3d), shape BitMaps, sound
  programs, frame counter (address escapes), RedStorm's NewWindow and a
  few RedStorm/Title/RedStart bytes not yet identified.
- **Item 3, styles and palettes (2026-11-15).** Text styles stay named by
  their address (the menus store it, the drawing code gets it), but
  screen::style_at(handle) returns the C++ object of a moved style
  (screen/styles.hpp: Storm's text_style, RedStart's page/row styles,
  Red's page/symbol styles); ui::State's style pointers are StyleRef,
  declarations StyleVar, same syntax. Red's map view and menu bounds are
  its symbol style's geometry. The library palette and the module
  palette are in ui::State; set_palette takes host colours. After this
  RSR_DATA_WRITES shows the module images changing only in: chip-side
  data (shape BitMaps, sound programs and set_tone's words, frame
  counter), Red's number_text/news_day area, the commander/sub names
  (with the records: 3d), RedStorm's NewWindow and a few unidentified
  RedStorm/Title/RedStart bytes. Next: 3d (the player and module
  records, heap objects), 3e (OS plumbing), 3f (constant data).
- **Item 3d, the player's record (2026-11-16).** The record is one C++
  byte array in its original layout (common/record.hpp, player.cpp: 502
  bytes; the loader allocates 148 and the original wrote the campaign's
  state past that, over the heap's free piece after it). Its emulated
  address stays the handle; career()/player()/battle_start()/
  battle_record() are views of the C++ bytes; copy_colours copies
  nothing (one record); saves copy file_buffer to and from it; scripts'
  `*a4+5234+M` reach it (NativeState::access_address); its allocation is
  a moved address range (state::move_address_range, checked by the
  trace). Gate PASS, recordings unchanged.
- **Item 3d, the module record (2026-11-20).** The loader's record for
  the modules is one C++ ModuleRecord (common/records.cpp, with the
  player's record; player.cpp renamed). Its emulated address stays the
  handle the loader puts in tc_UserData (redstorm::State::record is a
  be32 handle now); common::module_record(handle) is the object (aborts
  on another address); load_view/save_view/module_exit and the loader's
  fill_record/restore_from_record/run_* use it; scripts reach its bytes
  through access_address; its allocation is a moved address range. Gate
  PASS, recordings unchanged. 3d is done. Next: 3e (OS plumbing), 3f
  (constant data).
- **Item 3e.1, the module handles and start message (2026-11-20).** The
  loader's handles (loader.cpp Handle, four slots) and the Workbench
  start message (common/start_message: lock, name, tool window, reply
  port) are C++. Their memory is still allocated, so the heap keeps its
  layout and the addresses stay as handles (wb_message is still main's
  argv), checked as moved by the trace while they live
  (state::release_address_range when freed). PutMsg/WaitPort/GetMsg/
  ReplyMsg of that message only take their time (os::*_time); exec's
  lists in memory no longer carry it. The AV references do not depend on
  heap bytes (only the determinism check's RAM hash does). Gate PASS,
  recordings unchanged.
- **Item 3e.2, the process (2026-11-20).** common::Process
  (common/process): the task's name, tc_UserData (the module record's
  handle), pr_WindowPtr, pr_CLI (cleared while a module runs), current
  dir, console, stack bounds and the CLI's command name, taken once from
  the OS's task (os::process_start, the only place that reads the task's
  memory). common::find_task matches the name there and takes FindTask's
  time. Gate PASS, recordings unchanged.
- **Item 3e.3, ports and I/O requests (2026-11-20).** The game's ports
  (keyboard, the handles' reply ports) and requests (keyboard.device's)
  are C++ slots in common/devices.cpp; their memory and signals are still
  allocated (common::object_memory: heap layout kept, checked as moved
  while they live). keyboard.device's open/close only take their time
  (common::open_keyboard: the keys come from the key matrix). os:: no
  longer offers GetMsg/PutMsg/ReplyMsg/WaitPort/FindTask/OpenDevice: the
  game reaches exec's lists nowhere. Gate PASS, recordings unchanged.
- **Item 3e.4, the C allocator (2026-11-20).** Its blocks still come
  from AllocMem (addresses and heap layout are the original's); its
  records are C++ (common/memory.cpp): the free list per module with the
  lifetime of the initialised data it replaces (kept while a module stays
  loaded, as the original's was, even over blocks its last run gave back:
  that only happens with Storm resident and the animation scripts not
  preloaded, 645000-1000000 bytes free, and matches the original as long
  as nothing else reused that memory), the blocks and the malloc size
  words for the run (BSS). The block headers are moved ranges while they
  live; the image's free list is checked empty at load. No scenario
  mallocs in Storm (only RedStorm's two records and Title's one). Gate
  PASS, recordings unchanged.
- **Item 3e.5, the vertical-blank server (2026-11-20).** The frame
  counter is audio::State::frame_counter (scripts reach it at its old
  offsets); readers call audio::frames() instead of dereferencing the
  frame timer (its address stays in ui::State and the module record as
  a handle). The Interrupt node is no longer filled: the HLE's host
  servers take their priority as an argument (Hle::add_host_server), the
  node's address only names the server. Gate PASS, recordings unchanged.
  3e is done as far as the game goes: the stdio file list and fd table
  are never written (no file goes through the C library), so they are
  constant data (3f). RSR_DATA_WRITES over four scenarios now shows only:
  BitMap headers and plane pointers (shape and Storm's own BitMaps:
  chip side, g), Red's number_text/news_day area, the commander/sub
  names (Red +15996/+16028, RedStart +16256 and +14009/+14043), RedStart's
  protection patch ($202dc), intro_picture and text_pen bytes, RedStorm
  +899 and +10105, Storm -13301/-12581/-12567. Next: those leftovers,
  then 3f (constant data).
- **Item 3, the names (2026-11-20).** Red's commander and submarine
  names are red::Texts fields (main BSS; the rank messages promotion
  copies are at most 13 bytes with the 0, so 16 hold them); RedStart's
  commander name and save file handle are redstart::State fields;
  input_text types into a host buffer (GAME_L2_INSIDE). Gate PASS,
  recordings unchanged.
- **Item 3, RedStart's file buffer (2026-11-20).** The 500-byte buffer
  the save index, the honor roll and saved games are read into is a
  redstart::State field (BSS; scripts reach it at a4+14008); saves() and
  fames() view it; current_save/current_fame hold entry numbers instead
  of addresses (host pointers would make the state hash differ between
  runs); files are read and written from host buffers (os::write with a
  host buffer added). Gate PASS (with check_save_load), recordings
  unchanged.
- **Item 3, fonts and RedStart's third style (2026-11-20).** RedStorm's
  font segments are redstorm::State fields. RedStart's text style at
  a4-15648 (its pen set for the save and honor roll lines, the former
  text_pen_15636) is a third style slot (screen/styles.hpp). Gate PASS,
  recordings unchanged. What RSR_DATA_WRITES still shows in the images:
  chip side (BitMaps, sound programs: g), RedStorm's NewWindow (the
  display: Intuition reads it), RedStart's intro page's picture word
  (a4-29056, inside a menu page in the constant data: 3f) and the
  protection patch (code), Red's number_text/news_day.
- **Item 3, Red's number_text and news_day (2026-11-20).** One C++ char
  area in red::Texts (a4+15685..15981, Red's BSS) holds both, news_day
  41 bytes in, so lines built in number_text still run over news_day as
  in the original (red::number_text(), red::news_day()). Gate PASS,
  recordings unchanged. Item 3's game state is now out of the images
  except the chip/display side (g), the intro page's picture word (3f)
  and the protection patch. Next: 3f (constant data into host tables).
- **Item 3f.1, constant data: the mechanism and RedStorm (2026-11-20).**
  mem/constants: when a module image is loaded (relocated, patched;
  NativeState::image_loaded) a host copy is kept; Const<T>, ConstLib<T>,
  ConstPerModule<T>, ConstData<T> (the Var family, read-only) and
  constant<T>(addr)/const_text(addr) (through stored addresses) read the
  copy. In the trace run every read is compared with the emulated memory
  ("CONST CHANGED", a gate failure; tested by flipping a byte), so a
  table the game writes cannot pass for a constant. RedStorm's texts and
  file names are Const; the loader builds paths as host strings
  (path_join returns std::string), os::open_library/lock/load_seg take
  host names, find_task takes a host name. Stays Var: NewScreen/NewWindow
  and the screen palette (Intuition and LoadRGB4 read them: display side,
  g). The start message's name stays an address (never read). Gate PASS,
  recordings unchanged. Plan for the rest of 3f, a subsystem per commit:
  common, audio, input, screen, Title, RedStart, Red, Storm; first the
  declarations (Var to Const: writes become compile errors), then the
  structures read through stored pointers (menus, text lists, pages) via
  constant<T>(addr); measure with the RSR_TRACE files (accesses in the
  initialised data by label).
- **Item 3f.2, the libraries' constants (2026-11-20).** common (sines,
  powers, blanks, ship names, degree template, growth unit, the stdio
  list and the never-written fd table), audio (channel units, effect
  routines, sample groups, sound file names), input (key tables), screen
  (file names, error texts, the requester's texts and palette, picture
  and animation tables, blit masks) are Const. Host strings through:
  read_file_part, disk_requester (the lock a host reference), anim_error,
  load_shape's message, need_volume, RedStart's save_disk_ok. The key
  matrix turned out to be state (the platform fills it): ui::State. Stays
  Var: the sound programs (the engine programs and set_tone's words are
  written, the player reads all programs by address), the requests, the
  shape BitMaps (chip side). Gate PASS, recordings unchanged.
- **Item 3f.3, menus, RedStart and Title (2026-11-20).** The menu code
  shared by RedStart and Red reads its structures from the copies: row
  menus, entry lists (ui::State::menu_list is a ConstPtr, a stored
  address of constant data), sequence pages and spans by their handles;
  menu_page takes host blocks (menu_page(addr) checks a page's blocks
  whole). RedStart's and Title's declarations are Const (texts as host
  strings, palettes as host colours, the medal/month/level name tables
  through const_text). RedStart's intro page is the one page the
  original wrote (its text word, a4-29056): a local copy of its blocks
  with that word set. Gate PASS, recordings unchanged.
- **Item 3f.4, Red's constants (2026-11-20).** All of Red's declarations
  but word_134 and campaign_record are Const (tables, texts, palettes,
  missions and their rules, routes, outcomes, menus); the packed texts
  (message lists, text_n's list: ui::State::text_list is a ConstPtr) are
  unpacked from the copies; skip_strings, print_name, the plural and
  form_group work on host pointers. Trap found: address_of() of a Const
  element gave a garbage address (a host copy is not in the emulated
  memory); address_of now stops the program for any host object. Gate
  PASS, recordings unchanged.
- **Item 3f.5, Storm's constants (2026-11-20).** Every declaration of
  Storm's initialised data is Const but tracked_weapon (written). The
  ship type table turned out to be written in one entry: the battle sets
  the torpedo type's noise, defence and speed; that entry is
  initialised-data state (storm/state.cpp, a moved range), the others
  constants behind one ship_types[i]. Name tables go through const_text
  (short_name, announce and the status texts take host strings); the
  tactical analysis texts and ship lists are ConstPtr. Gate PASS,
  recordings unchanged.
- **Item 3f.6, stored pointers and RedStart's mixed variables
  (2026-11-20).** Measured with the trace (raw reads inside the module
  images, outside the C BSS; RSR_TRACE_STOP=MODULE:OFF stops at the first
  access for a backtrace, a new debugging aid). Red's group scripts,
  place routes and front segments and the UI's image list are ConstPtr
  (IMAGE_FIELDS knows ConstPtr). RedStart's own copies of the input
  library's per-module variables (menu bounds whose left word is
  menu_request, tick sounds, steering reference, no_return_fire, medals
  held: its initialised data) are native ui::State fields the
  PerModuleMixed declarations point to. In the trace run a constant read
  that overlaps moved state reports STATE MOVED (RedStart's medal_shapes
  at a4+0 overlap the menu bounds: draw_icons in RedStart is unreached).
  What game code still reads from the images in emulated memory: the
  sound programs (the player reads them by address; the engine programs
  and set_tone's words are written) and the chip side. Gate PASS,
  recordings unchanged.
- **Item 3f.7, sound programs and the last data (2026-11-20).** The
  programs the game writes (the three engine programs, set_tone's two
  words inside an effect program) are audio::State fields (initialised
  data, per-module offsets); the player reads every program word through
  program_word(), the state for those addresses, else the constant copy.
  Red's mission character lists (Mission::chars), nearest_of's list and
  place_pos's reads outside the places table come from the copies. The
  last written Vars are state: RedStorm's quit (redstorm::State), Red's
  word_134 (red::Data), Storm's tracked_weapon (initialised-data state).
  Trap found: ConstPtr<T> p{uint32} aggregate-initialised one byte
  (narrowing); ConstPtr has constructors now, and a clean build has no
  warnings. Measured over the 13 scenarios: L2 code reads nothing of the
  module images (inside a4-65536..+32767, the trace's window; the code
  hunks below are reached only through ConstData/ConstPtr) in emulated
  memory but RedStorm's NewWindow (Intuition's: display side). 3f is
  done. Gate PASS, recordings unchanged.
- **Resume here.** Items 3, 4 and 5 (a-f) are done; 5g (the memory
  image) is under way with the user's acceptance of new recordings (the
  gate keeps the old ones through --reference-terrain). Done in 5g: the
  OS plumbing (process memory, handles, ports, start messages), the
  shared state in native integers, then readability (names for every
  flag, kind, mode, key, pen, effect, weapon; long functions split; the
  port's history out of the comments; dead code; -Wall -Wextra; no
  m68k). Left: 5g.2 (file data in host buffers) and 5g.4 (the constant
  data as tables, the address space gone) - large, for little reading
  gain: ask the user before starting them.
- **Item 5a.1, the effect table (2026-11-21).** The effect routines are
  a static table of C++ functions (the table of code addresses was the
  same in every module: silence_all, silence, sound_on, the simple
  effects, the engine program switches); play_effect calls the routine
  directly through Machine::call_timed, which takes the time call_native
  took. Gate PASS, PAL runs equal to 11ecdbb.
- **Item 5a.2, no call by address (2026-11-21).** The running module is
  an explicit context (Machine::with_module: its number and load
  address) instead of the registration of the routine last entered. The
  loader runs a module's start-up as common::module_startup in that
  module's context; the start-up calls the module's main from a table of
  the five mains. The C runtime's atexit hook and break handler were
  never set by any module: their calls are gone. call_routine and
  call_native are gone; only Machine::run still finds RedStorm's entry
  by its registration. Gate PASS, PAL runs equal.
- **Item 5a.3, no registrations (2026-11-21).** GAME_L2, NATIVE_L2 and
  the like register nothing any more: they document where each function
  was in the original (tools/analysis/registrations.py still reads them).
  The hook tables (native_at_, native_flag_), the C-convention adapters,
  RSR_NATIVE_SKIP/ONLY and the gate's registration check and levels
  report are gone. The machine starts the program through
  NativeState::program_start (common::module_startup); a program file of
  another build than the WHDLoad data/ files is refused when loaded. Gate
  PASS, PAL runs equal.
- **Item 5b, names (2026-11-21).** Every variable, field and function
  that was named by its offset (63) has a name from its use: Storm's
  redraw flags per panel (redraw_helm, redraw_own_ship, ...), the
  battle's era and difficulty (from the battle start's +8 and +4),
  noise_floor, quiet_frames, the record's battles, units_left,
  final_rank and ending_rank, and so on. What nothing reads is named
  unread_* (kept: it is in the hashed state and the original's layout).
  The offsets stay in the comments. Gate PASS, PAL runs equal.
- **Item 5c.1-2, typed handles (2026-11-21).** Texts are passed as
  text (the overloads taking a text's address are gone). The bitmap
  drawing calls (fill_rect, blit, blt_bitmap, copy_region, draw_shape,
  recolour_rect, the graphics layer's blt_bitmap) take BitMap&;
  bitmap_at(n) is the BitMap. Text styles stay named by address: the
  menus' constant data points at them (with 5d). Gate PASS, PAL runs
  equal.
- **Item 5d.1, the menus' data typed (2026-11-21).** RedStart's and
  Red's menu pages, row menus, spans, sequence pages and entry lists are
  declared with their types (Const<PageBlock>, Const<RowMenu>, ...);
  menu_page, add_row, choose_row, draw_spans, page_sequence, set_menu
  and choose take them instead of addresses. Const::until gives a list
  up to its end marker. Gate PASS, PAL runs equal.
- **Item 5d.2-3 (2026-11-21).** Text styles are passed as TextStyle&
  (fill_style_fg/bg, draw_entry, fill_style, text_object); the last
  overloads by address for palettes and texts are gone; nearest_of takes
  its list. What still takes an emulated address: file buffers and
  pictures in the arena (unpack, build_shape), sound programs, the
  loader's handles and the record; these are memory the original
  allocated, kept for its layout.
- **Item 5e.1, no library bases (2026-11-21).** The os:: calls of
  dos, graphics and intuition no longer take the library base (the
  layer ignored it): os::open(name, mode), os::rect_fill(rp, ...). The
  bases stay in the state (the modules open and close the libraries:
  the heap's layout). The emulated OS layer itself (src/amiga, 3.5k
  lines) is small and its calls' timing is part of the recordings; it
  stays as the game's platform layer. Gate PASS, PAL runs equal.
- **Item 5f, the per-module copies (2026-11-21).** The C++ already has
  one function for each routine the modules share. What is per module
  is state: each program has its own copy of the library's variables
  (ui, audio, the allocator, the decoder), as the original's separate
  programs did; merging them would change what a module finds when it
  starts, so they stay. The module numbers are named
  (native::ModuleNumber). Gate PASS.
- **Bug: the year menu's highlight (2026-11-21, reported by the user).**
  On RedStart's year menu the highlighted entry kept a red strip at the
  left of each line (pixels 28-31 below the first line). The highlight
  (recolour_rect) is 310 pixels wide from x 28, so each blit line wraps
  onto the next line's first word, and its in-place passes (the mask
  plane read and written back) cleared that word's bits before the next
  line read them. The real blitter writes each D word one word late
  (after the next word's source reads); Machine::blit now does too.
  Only the year menu's 4 blocks (frames 2520-2700, picture only) of 11
  scenarios changed; re-recorded. Gate PASS.
- **Bug: dots in the typed name (2026-11-21, reported by the user).**
  After the identification test, the typed name showed a dot after each
  letter and dots for spaces. Font 2 is proportional with a kerning of
  1: each character's cell is kern + space pixels, its glyph 1 pixel in.
  Drawing::rp_text painted the JAM2 background from the glyph, so the
  cell's first column kept what was there: the '_' cursor's pixel. It
  now works as graphics.library's Text: the string is a template of the
  cells, the background painted over them wherever no glyph pixel is
  set. The same columns had let the picture show through the top row of
  the message pages' text (campaign 11400-11640, campaign_battle and its
  variants 48540-50820). 81 blocks of 11 scenarios re-recorded, all of
  them these two screens. Gate PASS.
- **Windows (2026-11-21, the user's request).** The code cross-compiles
  with MinGW unchanged; tools/package-windows.sh makes
  dist/RedStormRising-windows.zip (rsr.exe with the C++ runtime linked
  in, SDL3.dll, the shaders, run.bat, keys.pdf, a README for players).
  main.cpp: a `data` folder next to the program is the default game
  directory, saves go to %APPDATA%\RedStormRising on Windows, the shaders
  are looked for next to the program. Under Wine all 13 scenarios give
  the recordings exactly (picture and sound) and the save and recall
  test passes; the package found its data, shaders and save folder.
- **The archive read directly (2026-11-21, the user's request).** From
  the assets branch: its LHA reader (-lh5-, CRC-checked) and the table
  of the 21 files the game uses (size, FNV-1a), as
  src/host/game_archive.cpp; not the program images (the game keeps
  loading the original program files). rsr takes the archive on the
  command line or finds it next to the program, extracts the files once
  into the user's folder (data/ beside the saves) and plays from there;
  an archive given is always checked, another build refused. Tested on
  Linux and with the Windows package under Wine (the .lha next to
  rsr.exe, no argument): recordings exact.
- **Launch screen (2026-11-21, the user's request).** src/launcher.cpp,
  SDL only (renderer, SDL_RenderDebugText): the CRT look, fullscreen or
  a window, Start / Quit; mouse and keys; choices in launcher.cfg in the
  user's folder; missing game files shown there. Shown when the program
  starts without options; --launcher / --no-launcher. Tested headless
  (RSR_LAUNCHER_SHOT, RSR_LAUNCHER_KEYS) on Linux and under Wine.
- **Licence (2026-11-21, the user's request).** GPL-3.0-or-later, as the
  fa18-interceptor-port code it builds on requires: LICENSE at the top,
  a License section in README.md, SPDX lines in every source file (175;
  Ingo Ruhnke credited as well on src/amiga, src/present, main.cpp, the
  build and the shader tests), and in the Windows package LICENSE.txt
  and source.zip (git archive of the commit it is built from; the script
  refuses uncommitted changes).
- **ADF support (2026-11-21, the user's request).** The v843.02 disks
  (Skid Row crack) hold the WHDLoad build exactly, except RedStart's
  cracked beq (4E71 at file $2f8), which the reader puts back.
  src/host/game_archive.cpp reads OFS/FFS ADFs (tested on the real OFS
  disks and FFS copies made with amitools) next to the LHA. v843.01 is
  another build and is refused; its disks were deleted at the user's
  request.
- **Where item 5 stands (2026-11-21).** a-f done as far as they go
  without changing behaviour. What keeps the code Amiga-shaped: state
  in big-endian fields at the original's a4 offsets (hashed, read by
  scenarios, kept in the original's layout), file data and pictures in
  the emulated arena by address, the OS layer's timing. Each of these
  is tied to the recordings (heap layout: Storm reads its relocated
  code as sea-bed data); going further means accepting changed
  recordings or capturing those reads as data, the user's decision.
- **The user's decision (2026-11-21): accept new recordings.** The
  sea bed may follow wherever Storm is loaded (as on a real Amiga, where
  the load address depends on the machine's memory). Plan in 'Item 5g
  plan'.
- **Item 5g.0, the reference terrain (2026-11-21).** area_level's sea
  bed is Storm's own program read as terrain (bytes at a4-101850 on,
  & 7), so it changes where relocated addresses lie when Storm moves.
  --reference-terrain (the gate, record_reference.sh, check_reference.sh)
  takes those bytes from Storm's file as LoadSeg puts it at $09e4f8,
  where every recording had it (HunkFile::image_at, also LoadSeg's own
  code now). The game itself reads the image as loaded. With the
  option a change of the memory layout leaves the recordings as they
  are; without it only the terrain differs. Checked: with the
  reference moved 64 KB, battle_ice differs from frame 10020. Gate
  PASS.
- **Item 5g.1a (2026-11-21).** The memory CreateProc took for a module
  (process, name, stack) and the two impossible "flush" allocations are
  gone. Storm now loads at $08fa2c, Red at $06ff94: every recording is
  unchanged with --reference-terrain (the OS calls' 12 cycles each did
  not move a frame). Gate PASS.
- **Item 5g.1b, no process plumbing (2026-11-21).** A module's handle is
  its segment list; the loader calls common::module_startup(&args) with
  the start arguments (the directory's lock, the name) in the module's
  context. The 22-byte handles, their reply ports, start messages and
  arguments, the start-message hand-over (start_message.cpp), the
  hidden CLI and the tool window path are gone. The hand-over's OS calls
  still take their time, in one place (handover_time_before/after in
  the loader): without it every scenario past Title differs from frame
  2700 (one scanline per waiting call); with it all 13 are as recorded.
  Gate PASS.
- **Item 5g.1c, no ports or requests (2026-11-21).** keyboard.device's
  message port and I/O request (allocated, passed along, never read)
  are gone with the C runtime's port and request code: open_keyboard()
  and close_keyboard() keep the device calls' time; keyboard_io is 1
  while it is open. Recordings unchanged. Gate PASS.
- **Item 5g.3a, native state fields (2026-11-21).** The shared
  library's state (ui, common, audio, the loader's, RedStart's): every
  plain big-endian field is a native integer (92 fields); the field
  tables say so (XXX_N, MODULE_FIELD_N: native integers of an element
  size, arrays included, converted from the image when initialised).
  Two stay big-endian because the original reads them through their
  address: audio's frame_counter (the modules' frame timer) and tones
  (words of a sound program); as native they changed battle_kashin from
  frame 20400. Pointers and handles (Ptr, StyleRef, BitMapRef) stay
  as they are. Gate PASS; PAL runs equal to 11ecdbb (new side with
  --reference-terrain).
- **Item 5g.3b (2026-11-21).** The animation player's state too: its
  scalars, and the objects and region lists (in the BSS) with native
  fields. Gate PASS.
- **Item 5g, comments (2026-11-21).** The code's comments no longer
  refer to the port's history (this file's items and steps, L0/L1/L2,
  thunks.cpp, readme-translate-to-cpp.md): that history is here. The
  unused emulated-memory declarations (the audio device's requests,
  Storm's input.device variables) went before. Gate PASS.
- **Names of the documentation macros (2026-11-21).** GAME_L2 is
  ORIGINAL, NATIVE_L2 and NATIVE_L2_ALSO are ORIGINAL_IN(module, addr,
  fn), GAME_L2_INSIDE is ORIGINAL_INSIDE, AUDIO_L2 is AUDIO_ORIGINAL;
  check_l2.py is tools/analysis/check_code_rules.py. Gate PASS.
- **Key names (2026-11-21).** The key tables (the same in every module)
  give the special keys characters 128-174: the cursor keys, Help 136,
  F1-F10 145-154, Shift-F1-F10 155-164, Alt-A/E/P/Q/R/S/T/V/Z 165-174
  (read from the tables). input/vars.hpp names them (kKeyF1, kKeyAltQ,
  ...); Storm's key dispatch uses the names. The old kHelp (168) was
  Alt-P; key169_flag is quit_key (Alt-Q). Gate PASS.
- **Storm's flag bits named (2026-11-21).** Ship::flags (kCavitating,
  kTowedArray, kMastRaised, kAlerted, kNoisy, kDetected, kDamaged,
  kAfloat), Ship::flags_7 (kIdentified, kPlayerContact, kTracksPlayer),
  Ship::events (kEventLaunched - bit 1 was "hit" in the old comment, it
  is set when a ship fires - kEventDamaged, kEventSunk), Object::flags
  (kRunning, kWired, kTurnsRight): every use in Storm, the variable's
  type resolved for each. Gate PASS.
- **More of Storm by name (2026-11-21).** The damage bits (towed array,
  passive sonar, tubes, propulsion; sunk, lost), contact flags, log
  entry kinds, ShipType weapons, the tube's loaded bit; the sound
  effects (per module: each module plays its own samples, so Storm's
  and Red's numbers are named in their own headers); the event
  pictures; ShipType kinds (1 submarine, 2 surface ship, 3 aircraft:
  the old kSubmarineKind = 3 named the aircraft) and object kinds; the
  non-tactical displays and the panels' pages; the player's weapons
  from the game's own name table (Mark 48, Sea Lance, Harpoon,
  Tomahawk, TASM, Stinger). Red's unit flags by name everywhere. Each
  commit gate PASS.
- **Readability (2026-11-21, continued).** Red's war situations and the
  mission nibbles named; the projectile's side/flags/speed fields;
  read_stick decodes the directions directly (the JOY1DAT table is
  "opposite directions give none": all 16 cases checked); src/README.md
  maps the code; Storm's 32 battle pens named by colour (the battle
  uses one palette; pen 62 is dark grey's shadow pen); battle_main and
  tactical_display split into named steps (the ship pass hands the last
  heading and pen to the weapon pass, as the original's registers did).
  Each commit gate PASS.
- **native -> runtime (2026-11-21).** The namespace and directory
  `native` (C++ as opposed to 68000 code) are `runtime` (src/runtime:
  the running program's context); the build's object library is `game`;
  Machine's native_block/native_module are module_block/module_number,
  native_module_loaded is module_loaded, NativeState/native_state are
  GameHooks/game_hooks, native_dispatch.cpp is amiga/modules.cpp. Gate
  PASS.
- **Dead code (2026-11-21).** A -ffunction-sections --gc-sections build
  lists what nothing reaches: the OS layer's message, port, signal and
  list plumbing, the joystick input (none is connected: joystick()
  reports nothing), the address-taking overloads, the object memory of
  the ports and requests, and six of the original's routines no game
  path calls (clear_to_end, names_again, set_row_hook, set_text_list,
  unpack_file, frame_tick_wait: a note where each was). Removed (216
  lines); the same build now lists nothing. Gate PASS.
- **No m68k (2026-11-21).** Machine no longer derives from m68k::Bus
  (with no flat RAM every access went through its I/O path anyway): its
  read*/write* resolve the 24-bit address through bytes(). The
  registers and clock the game's conventions still use (a4, d0, the
  interrupt mask, the cycle count) are amiga::Cpu (amiga/cpu.hpp);
  src/m68k is gone. The game builds with -Wall -Wextra (the --ntsc help
  line's "20% faster" was a printf conversion). Gate PASS, PAL runs
  equal to 11ecdbb.
- **Item 4a, the display (2026-11-20).** The copper and the display
  registers' write path are gone. RethinkDisplay keeps the shown screens
  as Hle::Band (wait line, display window, fetch, planes, modulos,
  colours, DspIns); LoadRGB4/SetRGB4 update a band's colours;
  Machine::render_frame applies a band's registers at its wait line and
  no planes from its end, as the copper list did, and Denise's registers
  (Machine::Denise) keep their values from frame to frame (colour 0
  outside the screens, the pointer's colours 17-19). Same frames: gate
  PASS (NTSC), and PAL runs of four scenarios compared with the previous
  build (a worktree): same AV hashes.
- **Item 4b, the blitter (2026-11-20).** amiga::Blitter (blitter.hpp)
  holds the blitter's registers as typed fields; Machine::blit(size) runs
  the same blit and line logic on them at once (the pointer and modulo
  masks applied as the registers did). The game's blitter routines
  (copy_region, draw_shape, recolour_rect) set the fields and call
  hw::blit; the custom register writes for the blitter, the busy waits
  and the blitter-hog DMACON writes are gone. Gate PASS, PAL runs equal.
- **Item 4c, audio (2026-11-20).** Paula's channels are Machine members;
  audio.device starts and stops them directly (Machine::audio_start/
  audio_stop) instead of through DMACON's channel bits; the audio
  register writes are gone. The mixer (per-scanline stepping, the RC
  low-pass) is unchanged. Gate PASS, PAL runs equal.
- **Item 4d, ports and the register file (2026-11-20).** The game reads
  the joystick through platform::joystick() (the same JOY1DAT word is
  decoded; nothing connects a joystick); the custom chip and CIA register
  file is gone (custom_read/custom_write, the mouse counters, DMACON: the
  display is on once a display was made, Machine::display_dma_). What
  stays of the chips' state: the interrupt bits that dispatch the
  vertical-blank server. An access outside RAM reads 0 and logs "[io]";
  the gate fails on it. Gate PASS, PAL runs equal.
- **Item 4e.1, chip memory (2026-11-20).** The planes (the screen's,
  the loader's bitmaps, animation shapes, the shape buffer with the
  temporary plane) and the samples are in a chip region above RAM
  (0x800000-0xFFFFFF), one host arena (Machine::chip_alloc/chip_free,
  MEMF_CHIP, AllocRaster); the display, the blitter, graphics' drawing,
  Paula, the decoder, the shape mask, file reads and CopyMem reach bytes
  through Machine::bytes / mem::host_bytes (so do view() and the byte
  helpers). A bus access there is "[io]". The blitter's and display's
  pointer masks are 24-bit (equal to the old 2 MB masks below 2 MB).
  Two traps found: (1) the blitter reads a word past a shape row's end
  and writes back a word before a clipped destination (masked, without
  effect): blocks share one arena so those touch a neighbour as in RAM;
  (2) the modules' load addresses matter: Storm's area_level reads its
  own relocated code as sea bed, so every chip block still reserves its
  size in the RAM heap (the layout and the load addresses stay the
  original's; remember this for 4e.5). Gate PASS, PAL runs equal.
- **Item 4e.2, fonts (2026-11-20).** Drawing::Font: a TextFont read once
  from its segment (sizes, baseline, glyph bitmap, locations, spacing,
  kerning) into a host object named by its address; Text, TextLength and
  SetFont use it (fonts of an unloaded segment are forgotten). Gate
  PASS, PAL runs equal.
- **Item 4e.3, file data (2026-11-20).** The chip region became the host
  arena (Machine::arena_alloc); the preloaded files, the offset tables
  and the file buffer are in it too (os::alloc_data: the RAM heap still
  reserves their place). The game reads them through view/ptr_to (which
  resolve the arena). preload() named its file by address (a 3f
  leftover): a host string now. Gate PASS (the scenarios preload every
  file), PAL runs equal.
- **Item 4e.4a, BitMaps (2026-11-21).** amiga::BitMap (bitmap.hpp) is a
  host object named by the address of the original's 40 bytes:
  Machine::bitmap(handle) (made by InitBitMap/OpenScreen, else read once
  from those bytes; forgotten when their memory is freed). The display
  bands, graphics' drawing (Bits from a BitMap; BltMaskBitMapRastPort
  also from a BitMap that is no one's: draw_image's), the game's blitter
  routines, the decoder and the shape code use it; game handles are
  BitMapRef (ui::State::bitmaps, AnimObject::shapes), declarations of
  BitMaps in module data are BitMapSlot (screen/bitmap.hpp). While a
  BitMap is a host object its 40 bytes are a moved address range (the
  trace run reports an access: NativeState::bitmap_moved). draw_image
  still writes its BitMap's bytes on the stack (the stack's contents
  matter until 4e.5). Traps found: shape_mask was not a variable but
  shape_bitmap's plane 7 (now shape_mask()); start_sequence copied
  shape_bitmap into anim_shapes[0] as bytes. A wrong shape size made an
  empty region, and copy_region's BLTSIZE 0 blit (1024 x 64 words) ran
  170 KB on through the arena: so the arena now mirrors the RAM layout
  (a block's address is ARENA_BASE + its reserved RAM place; outside the
  blocks an arena address is that RAM; a freed block's bytes go back to
  RAM), and overruns reach what they reached in the original. Gate PASS,
  PAL runs equal.
- **Item 4e.4b, RastPorts (2026-11-21).** amiga::RastPort (rastport.hpp)
  the same way: Machine::rastport(handle), made by InitRastPort in
  OpenScreen/OpenWindow, its 100 bytes a moved range
  (NativeState::object_moved for BitMaps and RastPorts); graphics'
  drawing reads and sets its pens, modes, pattern, pen position and
  font; the game's handle is RastPortRef (ui::State::rastport). Found:
  RedStorm took the screen's BitMap from the screen RastPort's bitmap
  field (screen + 88, named kScreenBitMap): now kScreenRastPort and the
  host RastPort. A handle 0 stops the program. Gate PASS, PAL runs
  equal.
- **Item 4e.4c, screens and windows (2026-11-21).** amiga::Screen (with
  its ViewPort, RasInfo's BitMap and ColorMap) and amiga::Window
  (intuition_objects.hpp) are host objects named by the addresses the
  original's had (their memory, RasInfo's, ColorMap's and the DspIns
  token's are still allocated; the Screen's and Window's are moved
  ranges). IntuitionBase's active screen and window are Hle members; the
  ViewLord and GfxBase's ActiView are gone (build_bands works from the
  open screens). LoadRGB4, SetRGB4, GetRGB4 work on the host ColorMap.
  The game: ScreenRef (ui::State::screen), show_bitmap and colour() set
  the host ViewPort/ColorMap, colour cycling and the requester read the
  host table, os::active_screen and os::pal_display replace reads of
  IntuitionBase and GfxBase, the vertical-blank server reads the host
  window's mouse position, RedStorm the host window's RastPort. Gate
  PASS, PAL runs equal.
- **Item 4e.4d, the display calls' arguments (2026-11-21).** OpenScreen
  and OpenWindow take typed amiga::NewScreen/NewWindow, which RedStorm
  builds from its constants (NewScreenImage/NewWindowImage) with the two
  fields it changed in memory set on the host struct (the PAL top, the
  window's screen); SetPointer takes the image's words (read from the
  constant copy; the window keeps them for the sprite); LoadRGB4 takes
  host colours. The HLE no longer reads the modules' images. Found:
  ui::State mapped RedStorm's pointer_8x8/7x7/none to a4-3960/-3996/-3920,
  which in RedStorm are the pointer images themselves (no RedStorm code
  reads those fields): unmapped there. Gate PASS, PAL runs equal.
  Next: 4e.5 (module images only as host copies at their original
  addresses, no emulated stack or heap, no emulated RAM).
- **Item 4e.5a, the game's last data in RAM (2026-11-21).** Measured with
  a probe of Machine::bytes (RAM reached by game code, by module and
  a4 offset; RSR_BYTES_STOP=addr for a backtrace, a scratch tool): left
  were RedStart's three cleared variables (now redstart::State fields),
  malloc's blocks (the C allocator's blocks are arena blocks now:
  animation scripts, the records' memory), and the Scratch buffers on
  the emulated stack: all gone (host locals; picture_palette and
  get_system_time take host buffers, the alert is a host string,
  RedStorm's main gets no argv: it reads none). Storm's unset local was
  already a constant (draw_grid), so nothing reads stack words it did
  not write; the stack comments said otherwise and are corrected.
  Left for the game: the BitMap/RastPort imports and the blitter's reads
  of RedStorm's image planes (module images, 4e.5c), two null-pointer
  reads (an AnimCommand and a TextStyle at address 0: low memory, 4e.5b).
  Gate PASS.
- **Item 4e.5b, calls by address without the stack (2026-11-21).** Every
  routine the game still calls by address (the modules' start-up and
  main, the effect routines, the exit routine, the break handler) takes
  no arguments, so call_native runs it directly (Machine::run_native:
  the module context, the function; the same scanline boundaries and
  the return stub's 4 cycles): no return address, no rebuilt link/movem
  frame, no pushed arguments (call_routine and call_main take none;
  call_routine_on_stack is gone). The module processes' stacks are still
  allocated (heap layout). Low memory: all zeros but exec's base at 4
  (0x714, set by the HLE's low allocations, so they stay as they are):
  what the two null-pointer reads see. Gate PASS, PAL runs equal.
- **Item 4e.5c, module images in host blocks (2026-11-21).** LoadSeg
  loads and relocates a module into a host block at its load address
  (Machine::image_add; RAM keeps the address range: the heap's layout
  and the load addresses). The bus takes the io path on pages that
  overlap an image (Bus::special_page), where Machine::bytes resolves
  them, so HLE and script accesses reach the image; the game's memory
  helpers (strings, bytes, BSS clearing, host_text) all go through
  host_bytes (mem::ram() and address_of are gone). Gate PASS, PAL runs
  equal. What RAM still holds: low memory (the HLE's library bases,
  task, CLI; exec's base at 4, read by the two null-pointer reads), the
  structures the HLE allocates and clears (contents unused), heap
  reservations.
- **Item 4e.5d, no flat RAM (2026-11-21).** The 8 MB RAM array is gone.
  The address space below RAM_SIZE is low memory (a host block owned by
  the machine: the HLE's library bases, task, CLI, boot stack), the
  modules' image blocks, and pages of the rest made when first touched
  (cleared): no object lives there any more, only what the original
  left for its overruns to reach (the arena mirrors that space, freed
  arena blocks copy their bytes back). The bus has no RAM (ram_size 0):
  every access is resolved by Machine::bytes, whose fast path is a cache
  of 256-byte granules lying wholly in one place (low memory, an image,
  a stray page, an arena block); render_line resolves each plane's line
  once. Speed: campaign_battle 4.9 s against 4.4 s before item 4e
  (Release). Gate PASS, PAL runs equal. Item 4 is done. Gate: `bash tests/check_native.sh DIR` (about a minute), its
  last line must be PASS.

## Unverified

Native routines no scenario executes (`coverage.py` prints the list as
"never executed in any module"), with the reason they stay that way:

- No scenario: battle_won's other mission types (`targets_sunk`,
  enough_units there).
- No scenario: `hunt_check` (a hunting group at its destination).
- No scenario: `end_phase` (the war's end: setting a4+15502 before the
  battle does not survive to battle_result).
- No scenario: RedStorm's end (`quit_game`, `close_display`,
  `close_libraries`: the scenarios stop the emulator instead) and its
  paths for less memory (each module loaded before its run).
- No scenario: `unpack_file` (pictures decoded straight from a file).
- No scenario: `alert`, `fatal_error` (missing files, no memory), the
  write() path (`c_write`, `dos_write`, `c_lseek`, `dos_seek`,
  `fd_entry`: no buffered file is written at exit).
- C runtime, to be deleted (section 9): `parse_int`, `utoa_dec`,
  `utoa_oct`, `utoa_hex`, `strupr_` (printf conversions the game never
  uses), `pool_free`; `alert` (fatal-error requester).
- Need `--protection` (scenarios run with the bypass): start_main's
  wrong-answer branch and `wrong_answer`. Backspace in `input_text` (no
  scenario types one; adding it would change the campaign scenarios).
- Timing that a script cannot hit: `weapon_line` (Storm $2d080: an event
  picture while a number is being typed).
- Set before a scenario can change it: `random_conditions` (Storm
  $223d6: sea type a4-7088 = 19 is chosen while Storm starts).
- Red L2 with paths no scenario reaches: `award_medal` (only its early
  return runs; setting the score a4+14606 high in campaign_battle does
  not lead there), the turn's meeting with the target, `news_report`'s
  second-side tables.
  `port_screen`'s choices (campaign_base leaves the port at once; Down
  and Return there add only 35 instructions).
- Not found yet; candidates for more scenarios: `promotion` and
  `string_width` (medal and promotion paths at the end of a mission:
  need a4+15518 set and a successful mission), `red_count_class`,
  `draw_icons` and `print_header` (Red screens $235e8/$237aa, not
  identified), `red_reverse_group` (needs map time after the status
  report with a4+15658 > 0; the scripted pointer cannot reach the exit
  line there yet), `clear_to_end`, `controls_wrap`, `flag_names`
  (RedStart $209f4, now L2).

Routines reached in `--native off` but shown as "not reached" by the
per-call verify (`flush_keys2`, `format_thousands`, `frame_tick_red`,
`lzw_code`, `random_step`, `red_copy_view`, `udivmod`, `unpack_file`)
are called directly from native callers, not through their hooks: they
are checked as part of those callers.
