# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
import sys, os
sys.path.insert(0, os.environ.get("FA18_TOOLS", "../fa18-interceptor-port/tools"))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import loadseg, oscalls
import re
# The disassembler prints the displacement of indexed modes in hex without
# a prefix (a0@(18,d0:l) is +$18, while a0@(18) is decimal): add 0x.
INDEXED = re.compile(r"@\((-?)([0-9a-f]+),(%[ad][0-7]:[wl])\)")
hunks, addrs, img = loadseg.load(sys.argv[1], patched=False)
base = loadseg.LOAD_BASE
code = [(a, a + h.alloc_size) for h, a in zip(hunks, addrs) if h.kind == "CODE"]
# objdump disassembles linearly and loses the instruction boundaries after
# padding words or data (the 0000 word after an rts, jump tables), so whole
# routines can be missing from the listing. Resynchronise: disassemble
# again from every address that must be code (after rts + 0000; call
# targets that are not instruction starts) and splice that run in up to
# where it meets the main sweep again. An entry straddling the start
# becomes .short words.
import bisect, subprocess, tempfile
END = max(hi for lo, hi in code)
def in_code(a): return any(lo <= a < hi for lo, hi in code)
def word(a): return (img[a - base] << 8) | img[a - base + 1]
def window(start, stop):
    objdump = os.environ.get("OBJDUMP", "m68k-linux-gnu-objdump")
    with tempfile.NamedTemporaryFile(suffix=".bin") as tf:
        tf.write(img); tf.flush()
        out = subprocess.run([objdump, "-b", "binary", "-m", "m68k:68000", "-D",
                              f"--adjust-vma={base:#x}", f"--start-address={start:#x}",
                              f"--stop-address={stop:#x}", tf.name],
                             capture_output=True, text=True, check=True).stdout
    return [(int(m.group(1), 16), m.group(2), m.group(3)) for m in map(oscalls.LINE.match, out.splitlines()) if m]
insns = {a: (op, args) for a, op, args in oscalls.disassemble(img, base) if in_code(a)}
for _ in range(6):
    starts = set()
    for a, (op, args) in insns.items():
        if op == "rts" and in_code(a + 4) and word(a + 2) == 0 and (a + 4) not in insns:
            starts.add(a + 4)
        if op in ("jsr", "bsrw", "bsrs", "bsr"):
            m = re.match(r"(?:%pc@\()?0x([0-9a-f]+)\)?$", args)
            if m and in_code(int(m.group(1), 16)) and int(m.group(1), 16) not in insns:
                starts.add(int(m.group(1), 16))
    starts = sorted(starts)
    if not starts: break
    for st in starts:
        if st in insns: continue
        keys = sorted(insns)
        run = window(st, min(st + 0x400, END))
        sync = None
        for a, _, _ in run:
            if a > st and a in insns: sync = a; break
        if sync is None: continue
        # the main entry straddling st: as .short words up to st
        i = bisect.bisect_left(keys, st) - 1
        if i >= 0 and keys[i] < st:
            for k in range(keys[i], st, 2): insns[k] = (".short", f"0x{word(k):04x}")
        for k in keys[bisect.bisect_left(keys, st):bisect.bisect_left(keys, sync)]: del insns[k]
        for a, op, args in run:
            if a >= sync: break
            insns[a] = (op, args)
with open(sys.argv[2], "w") as f:
    for a in sorted(insns):
        op, args = insns[a]
        f.write(f"{a:06x}: {op} {INDEXED.sub(lambda m: f'@({m[1]}0x{m[2]},{m[3]})', args)}\n")
