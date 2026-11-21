Most of these tools measured the translation from 68000 code to C++
(the translation era: progress.py, ready.py, liveness.py, leaves.py,
copies.py, opdiff*.py). They read the ORIGINAL/ORIGINAL_IN lines through
registrations.py. In the gate: check_code_rules.py.

# Analysis scripts for the native translation

Quick tools written while translating (see `docs/history/restructure-strategy.md`).
They work on disassembly listings in `$RSR_DIS` (default `build/dis/`),
one per module, named `storm.s`, `redstorm.s`, `title.s`, `redstart.s`,
`red.s`, and need fa18's tools (`$FA18_TOOLS`, default
`../fa18-interceptor-port/tools`) and `m68k-linux-gnu-objdump`.

    mkdir -p build/dis
    for m in Storm RedStorm Title RedStart Red; do
      python3 tools/analysis/dis.py extracted/RedStormRising/data/$m \
        build/dis/$(echo $m | tr A-Z a-z).s
    done

- `dis.py EXE OUT`: linear disassembly of the code hunks (tools' layout,
  code at $20008). Linear: data inside code (jump tables) misaligns it;
  disassemble such ranges separately with objdump. Displacements of
  indexed modes are printed with `0x` (`a0@(0x18,%d0:l)`); objdump's
  own output leaves the hex prefix out there.
- `leaves.py LISTING`: routines that call nothing, by call count.
- `shape2.py MODULE ADDR...`: copies of routines in the other modules with
  the same instruction sequence (operands masked).
- `opdiff.py STORM_ADDR module:addr ...`: operand differences between a
  Storm routine and its copies (a4 offsets, absolute addresses; relative
  targets are ignored).
- `libcands.py`: Storm routines with copies in all four other modules,
  not yet translated (reads `done.txt` in `$RSR_DIS`: one hex address per
  line, e.g. from `grep -ho 'NATIVE_[A-Z]*("storm", 0x[0-9a-f]*' src/native/*.cpp`).

These are rough helpers, not maintained tools.
- `ready.py MODULE [N]`: untranslated routines whose callees are all
  native already (reads the registrations in `src/native/`): the next
  layer for a bottom-up translation.

- `opdiff2.py`: operand differences between copies of a routine in any two
  modules (a4 offsets, call targets, tables).
- `copies.py`: untranslated routines with the same instruction shape as a
  translated one in another module (candidates for `NATIVE_*_ALSO`).
- `calls.py MODULE ADDR`: each call of a routine with the return offset
  native code must pass (`self() + offset`) and its target. Copy the
  offsets from here instead of computing them by hand.
- `liveness.py`: what the callers of each routine read after it returns
  (condition codes, parts of d0/d1, a0, a1); `liveness.py MODULE ADDR`
  lists the call sites and where each read is. Also the disassembly
  parser `progress.py` uses. (Its table for `--native verify` went with
  the interpreter, step 4e.)
- `check_code_rules.py`: the rules for `src/game` bodies (in the gate; was check_l2.py).
- `progress.py`: share of the original instructions at L0/L1/L2 per
  module (in the gate).
- `statemap.py MODULE TRACE... [--range A B] [--decls]`: the declared
  variables of a module's small-data area, their overlaps, the extents the
  scenarios used, and raw accesses (stored pointers, byte copies, strings)
  into them, from `RSR_TRACE=1 tests/run_scenarios.sh` traces. For
  section 10 step 3 (state out of the memory image).
