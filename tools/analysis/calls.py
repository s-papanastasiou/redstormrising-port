# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The calls of a routine with the return offsets native code must pass
# (self() + offset = the address after the jsr/bsr), and their targets
# (jump stubs resolved):
#   python3 tools/analysis/calls.py MODULE ADDR
import os, re, sys
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
dis = os.environ.get("RSR_DIS", os.path.join(root, "build", "dis"))
mod, start = sys.argv[1], int(sys.argv[2], 16)
ins = []
for l in open(os.path.join(dis, mod + ".s")):
    m = re.match(r"([0-9a-f]{6}): (\S+) ?(.*)", l.rstrip("\n"))
    if m: ins.append((int(m.group(1), 16), m.group(2), m.group(3)))
stub = {a: int(r[2:], 16) for a, o, r in ins if o == "jmp" and re.match(r"0x[0-9a-f]+$", r)}
i = next(k for k, (a, _, _) in enumerate(ins) if a == start)
while i + 1 < len(ins):
    a, o, r = ins[i]
    nxt = ins[i + 1][0]
    if o in ("jsr", "bsrw", "bsrs", "bsr"):
        m = re.search(r"0x([0-9a-f]+)", r)
        t = int(m.group(1), 16) if m else None
        via = f" (-> {stub[t]:06x})" if t in stub else ""
        print(f"  +{nxt - start:#05x}  {o:5s} {r}{via}")
    if o == "rts": break
    i += 1
