# Reading the code

## Every function says where it was

```cpp
// $22ccc: the score factor: total / divisor * multiplier / 4 * a term
// clamped to 1..11, in 16-bit steps.
void score_factor() { ... }
ORIGINAL_IN("red", 0x22ccc, score_factor);
```

- **The leading comment** names the routine's address in the original,
  in the tools' layout (a program's first code at `$20008`, as in the
  disassemblies `build/dis/*.s`, made by `tools/analysis/dis.py` with
  the sister project's tools), then what it does.
- **`ORIGINAL_IN(module, address, function)`** records the same for one
  program. **`ORIGINAL(function, storm, redstorm, title, redstart, red)`**
  gives the address in each program that has the routine, 0 where one
  does not. **`ORIGINAL_INSIDE`** marks a routine whose work is done
  inside another function. **`AUDIO_ORIGINAL`** is the sound code's
  form.
- All of these generate no code. `tools/analysis/registrations.py` reads
  them.
- Some of the original's routines are not reached by any game path.
  They are not translated, and a one-line note stands where they would
  be.

## The arithmetic is the original's

The game is reproduced exactly, frame for frame, so the code keeps the
original's behaviour where a straightforward rewrite would differ:

- **16-bit values.** The original worked in 16-bit words. The casts
  (`int16_t(a + b)`, `uint16_t(x >> 8)`) are where it truncated, and
  overflow happens where it happened.
- **The original's quirks.** It sometimes reads past the end of a
  table, keeps a register's value from the previous loop, or tests a bit
  two ways. Where the C++ reproduces such a thing, a comment says so
  ("as in the original").
- **Order.** Calls to the random number generator and to the OS stay in
  their original order: each changes what follows.

## State

Variables live in C++ structs, one per subject:

- **Storm:** `storm::battle`, `ships`, `objects`, `sight`.
- **Red:** `red::phase`, `groups`, `loadout`, `scores`.
- **The shared libraries:** `ui::state()`, `audio::state()`,
  `common::state()`, which return the running module's copy.

Their `state.hpp` files also hold tables that give each field's place
in the original program (its offset from the module's small-data base,
a4):

```cpp
    UI_N(menu_page_top, 2, 2, X, X, X, -15560, 12100),   // native; its a4 offset in each module (X: none)
    AUDIO_BE(frame_counter, 4, 8968, X, 4700, 29464, 16292),  // kept big-endian
```

The tables are what make the original's rules still apply to the C++
fields:

- the C start-up clears the module's BSS;
- initialised data is taken from the program file when it is loaded;
- the test scripts' `set` and `get` reach a variable by its original
  offset;
- the trace run fails if anything reaches a moved variable in emulated
  memory.

Most fields are native integers. Fields that the original reads through
their address stay big-endian (`be16`, `be32`, `sbe16`).

## Data from the game's files

The game's tables and texts are not in this repository. They are read
from the loaded program files and declared by their place and type in
each directory's `vars.hpp`:

```cpp
inline constexpr Const<RowMenu> menu_main{-30422};           // at a4-30422 of the running module
inline constexpr ConstLib<char> sounds_missing_text{-12620, Block::Bss};  // in the shared library's block
inline constexpr ConstPerModule<sbe16> channel_units{{-13406, 0, -27254, -20714, 6446}};
```

- `*menu_main`, `menu_main[i]` and `text.str()` read them through the
  copy taken when the program was loaded. Writing through a `Const` is
  a compile error.
- **Records in the original's layout** use the big-endian field types
  from `mem/types.hpp`, with their size asserted. Examples are the
  player's record, `BattleStart` and `ShipType`.
- Records saved to disk keep that layout, because the save files must
  stay compatible.

## Names

- **Constants are named after what the game does with them.** Where the
  game's own data says what a number is, the name comes from there:
  - weapon types from Storm's weapon name table (`kMark48` …
    `kStinger`);
  - pens from the battle palette (`kPenRed` is `$f22`);
  - key characters from the key tables (`kKeyF1`, `kKeyAltQ`).
- **Flags and kinds:** bit names per field. Examples are `kAfloat` and
  `kDetected` (`Ship::flags`), `kWired` (`Object::flags`), and
  `kDamagePassiveSonar` (`battle.damage_word`).
- **Fields nothing reads** are named `unread_*`. They are kept because
  they are part of the original's state and of the hashed memory.
- **Sound effects** are numbered the same in every program, but each
  program loads its own samples. So effects are named in each program's
  header: `storm::kEffectRadar`, `red::kEffectAlert`.

## The rules the gate checks

`tools/analysis/check_code_rules.py` rejects, in game code outside
`src/game/mem`:

- CPU state;
- raw memory access (`rd16`, `maddr`, `view<…>`);
- calls by address;
- numeric addresses or offsets: those belong in a declarations header.
