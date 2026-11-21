# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Untranslated routines that have the same instruction shape (operands
# masked, as shape2.py) as a routine already registered in another module:
# candidates for a NATIVE_*_ALSO registration (after checking the operand
# differences with opdiff-style comparison: a4 offsets and call targets).
#   python3 tools/analysis/copies.py
import os, re, glob, collections
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
dis = os.environ.get("RSR_DIS", os.path.join(root, "build", "dis"))
MODS = ["storm", "redstorm", "title", "redstart", "red"]
done = collections.defaultdict(set)
for f in glob.glob(os.path.join(root, "src", "native", "*.cpp")):
    for m in re.finditer(r'NATIVE_\w+\(\s*"(\w+)",\s*0x([0-9a-f]+)', open(f).read()):
        done[m.group(1)].add(int(m.group(2), 16))
ins, idx = {}, {}
for mod in MODS:
    l = []
    for line in open(os.path.join(dis, mod + ".s")):
        m = re.match(r"([0-9a-f]{6}): (\S+) ?(.*)", line.rstrip("\n"))
        if m: l.append((int(m.group(1), 16), m.group(2), m.group(3)))
    ins[mod] = l
    idx[mod] = {a: i for i, (a, _, _) in enumerate(l)}
def shape(mod, a):
    i, out = idx[mod].get(a), []
    if i is None: return None
    l = ins[mod]
    while i < len(l) and len(out) < 400:
        _, o, r = l[i]
        out.append(o + " " + re.sub(r"-?\d+", "N", re.sub(r"0x[0-9a-f]+", "A", r)))
        if o in ("rts", "rte"): return tuple(out)
        i += 1
    return None
known = collections.defaultdict(list)
for mod in MODS:
    for a in done[mod]:
        s = shape(mod, a)
        if s and len(s) >= 4: known[s].append((mod, a))
targets = collections.defaultdict(set)
for mod in MODS:
    for a, o, r in ins[mod]:
        if o in ("jsr", "bsrw", "bsrs", "bsr"):
            m = re.match(r"(?:%pc@\()?0x([0-9a-f]+)\)?$", r)
            if m: targets[mod].add(int(m.group(1), 16))
for mod in MODS:
    for a in sorted(targets[mod] - done[mod]):
        s = shape(mod, a)
        if s and s in known:
            print(f"{mod:9s} {a:06x} len={len(s):3d} = " + " ".join(f"{m}:{x:06x}" for m, x in known[s]))
