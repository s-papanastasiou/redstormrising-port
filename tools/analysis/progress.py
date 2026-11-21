# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# How much of the game is at each level (docs/history/restructure-strategy.md):
# L0 original code, L1 register-exact translation (src/native), L2
# readable C++ (src/game, ORIGINAL_IN). Counts the original instructions of
# each routine (a call target in build/dis up to the next one).
#   python3 tools/analysis/progress.py          one line per module and a total
#   python3 tools/analysis/progress.py --short  the total line only
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from liveness import Module, MODULES, ROOT
from registrations import registrations


def levels():
    return {k: lvl for k, (_, lvl) in registrations().items()}


def main():
    lev = levels()
    tot = [0, 0, 0]
    lines = []
    for name in MODULES:
        m = Module(name)
        n = [0, 0, 0]
        for k, e in enumerate(m.entries):
            if e in m.stub: continue
            end = m.entries[k + 1] if k + 1 < len(m.entries) else None
            i, size = m.idx[e], 0
            while i < len(m.ins) and (end is None or m.ins[i][0] < end):
                if m.ins[i][1] != ".short": size += 1
                i += 1
            n[lev.get((name, e), 0)] += size
        all_ = sum(n) or 1
        lines.append(f"{name} L0 {100 * n[0] // all_}% L1 {100 * n[1] // all_}% L2 {100 * n[2] // all_}%")
        tot = [a + b for a, b in zip(tot, n)]
    all_ = sum(tot)
    total = (f"progress: {all_} instructions; L0 {tot[0]} ({100 * tot[0] // all_}%), "
             f"L1 {tot[1]} ({100 * tot[1] // all_}%), L2 {tot[2]} ({100 * tot[2] / all_:.1f}%)")
    if "--short" not in sys.argv:
        print("; ".join(lines))
    print(total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
