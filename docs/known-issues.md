# Known issues

Behaviour a player may notice that is deliberately left as it is, with
what causes it and how it could be changed. Recorded so later work starts
from the analysis instead of repeating it.

## Mouse clicks that do not register

**Symptom.** In a battle, a click to fire a torpedo (or a missile) is
sometimes ignored and has to be repeated once or twice.

**Status.** The original game's behaviour, reproduced exactly; left as
is (2026-11-21). A 1990 Amiga loses the same clicks.

### Cause

Three things together:

1. **The game acts on the button's state, not on clicks.** Firing
   happens in `weapon_step` (`src/game/storm/ships.cpp`), which asks
   whether the select button is down *at that moment*:
   `controls()` reports `kFire` while `ui::state().select_button` is
   set (`src/game/input/input.cpp`).
2. **The button state comes from Intuition's mouse messages, read all at
   once.** `getkey` (`src/game/input/input.cpp`) reads every message
   waiting since its last call: button-down sets `select_button`,
   button-up clears it. If a click's down and up are both waiting, they
   cancel out: `select_button` ends at 0 and nothing fires.
3. **In real time the battle reads input in bursts.** Each pass of
   `battle_main` (`src/game/storm/battle.cpp`):
   - draws the battle;
   - reads the keys twice (`handle_keys`, `weapon_step`);
   - waits until `kTickFrames` (20) frames have passed since the last
     pass;
   - reads them three more times in quick succession.

   A click that starts and ends during the wait (up to about 0.4 s on
   PAL, 0.33 s on NTSC) reaches `getkey` whole and is lost.

So it depends on when in that cycle the click falls, and a short click
is more likely to be lost than a longer press. A highlight or pointer
reacting to the click does not mean it fired: the separate "a mouse
message arrived" flag (`ui::state().button_event`, `mouse_clicked()` in
Red) is set either way.

### Where else

The mechanism is the same everywhere; what differs is how often the
button is checked.

| Where | How often the button is checked | Which clicks are lost |
|---|---|---|
| Battle, real time (firing, aiming) | 5 times close together, then a wait of up to 20 frames | Any click that starts and ends inside the wait |
| Battle, accelerated time (Alt+T) | Every pass of the loop, one pass every 4 frames | Clicks that start and end inside those 4 frames |
| RedStart's row menus (`red::choose_row`: rank, era, difficulty, scenario, saves) | Continuously (`frame_tick` does not wait there: `mouse_steering` is off) | Clicks shorter than one frame |
| Red's two-column menus (`red::choose`: the control room, loadout, port) | Every 1-2 frames (`delay(1)`, `frame_tick`) | Clicks shorter than one frame |

**One frame is the front end's limit.** `src/main.cpp` hands SDL's
events to the game once per frame (`handle_events` in `SdlHost::frame`).
If a click's press and release arrive within the same frame (20 ms on
PAL), the game gets them together and they cancel.

- With a mouse, a click lasts 50-150 ms, so the menus see it.
- With a touchpad's tap-to-click, press and release often arrive at
  nearly the same instant, so a tap can fail every time.

**Deliberate:** the menus start with `flush_keys`, which forgets a
click made just before the menu appeared, as the original did.

### Workarounds for players

- Hold the button until the torpedo launches.
- Space or Return also fire, but they too only count while held.
- Accelerated time (Alt+T) checks much more often (every 4 frames).
- With a touchpad, click physically instead of tapping.

### Possible fix

Change how the machine delivers mouse messages (`src/amiga/intuition.cpp`,
`Hle::mouse_button` and `Hle::next_input`), not the game logic:

- When a button-up arrives while its button-down is still unread, hold
  the up back.
- `next_input` delivers the down, then reports the queue empty once, so
  that `getkey`'s loop ends with `select_button` set.
- The next `getkey` delivers the up.

The game then sees every click as a press, in the battle and on
touchpads alike.

Points to settle first:

- **Behaviour.** It departs from the original for mouse input, so it
  may be better as an option (for example `--hold-clicks`) than as the
  default.
- **Tests.** The scenarios use scripted clicks (`click` holds the button
  for 4 frames; `lmb 1` / `lmb 0` are explicit). The gate must show
  whether any recording changes. If one does, the option keeps the
  recordings as they are.
- **Keys.** Keys have the same issue: `controls()` reads the key matrix
  as it is at that moment. If wanted, the same hold-back could apply to
  key-up events.

## Accelerated time (fixed 2026-11-21)

**Was:** Alt+T in a battle ran time so fast that the battle was over in
a second or two.

**Cause:** the original's battle loop (`battle_main`,
`src/game/storm/battle.cpp`) waits for `kTickFrames` (20) between
passes only in real time. In accelerated time it does not wait at all:
a pass took as long as the A500's 68000 needed for it, a few frames. The
port's C++ needs almost no emulated time for the same work, so hundreds
of passes ran per frame.

**Fix:** in accelerated time each pass waits for
`kAcceleratedTickFrames` (4) frames, five times real time
(`src/game/storm/vars.hpp`). The exact A500 figure could not be
measured: the last builds that ran the original 68000 code do not take
keyboard input in that mode. 4 is an estimate; change the constant to
make it faster or slower. No test scenario uses Alt+T, so the recordings
are unchanged.

