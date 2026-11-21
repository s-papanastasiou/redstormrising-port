# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Untranslated routines whose callees are all native already (the next
# layer for a bottom-up translation).
#   python3 tools/analysis/ready.py MODULE [N]
# MODULE: storm, redstorm, title, redstart, red. Reads $RSR_DIS/MODULE.s
# (default build/dis/) and the registrations in src/native/*.cpp.
# A routine is a call target; its body runs to the next call target.
# Jump stubs (a lone `jmp addr`) are resolved. OS calls (jsr -n(a6)) do
# not count as callees. Prints: address, static calls, instructions,
# callees.
import os, re, sys, glob, collections

mod = sys.argv[1]
top = int(sys.argv[2]) if len(sys.argv) > 2 else 40
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
dis = os.environ.get("RSR_DIS", os.path.join(root, "build", "dis"))
done = set()
for f in glob.glob(os.path.join(root, "src", "native", "*.cpp")):
    for m in re.finditer(r'NATIVE_\w+\(\s*"(\w+)",\s*0x([0-9a-f]+)', open(f).read()):
        if m.group(1) == mod: done.add(int(m.group(2), 16))

ins = []
for l in open(os.path.join(dis, mod + ".s")):
    m = re.match(r"([0-9a-f]{6}): (\S+) ?(.*)", l.rstrip("\n"))
    if m: ins.append((int(m.group(1), 16), m.group(2), m.group(3)))
idx = {a: i for i, (a, _, _) in enumerate(ins)}
stub = {a: int(r[2:], 16) for a, o, r in ins if o == "jmp" and re.match(r"0x[0-9a-f]+$", r)}
CALL = ("jsr", "bsrw", "bsrs", "bsr")
calls = collections.Counter()
for a, o, r in ins:
    if o in CALL:
        m = re.match(r"(?:%pc@\()?0x([0-9a-f]+)\)?$", r)
        if m:
            t = int(m.group(1), 16)
            calls[stub.get(t, t)] += 1
entries = sorted(t for t in calls if t in idx)
res = []
for k, t in enumerate(entries):
    if t in done or t in stub: continue
    end = entries[k + 1] if k + 1 < len(entries) else None
    i, body = idx[t], []
    while i < len(ins) and (end is None or ins[i][0] < end):
        body.append(ins[i]); i += 1
        if len(body) > 2000: break
    callees = set()
    for a, o, r in body:
        if o in CALL:
            m = re.match(r"(?:%pc@\()?0x([0-9a-f]+)\)?$", r)
            if m:
                c = int(m.group(1), 16)
                callees.add(stub.get(c, c))
            elif "%a6" not in r and "%fp" not in r:
                callees.add(-1)  # indirect call
    if any(c not in done for c in callees): continue
    if not callees: continue  # leaves: see leaves.py
    res.append((calls[t], t, len(body), sorted(callees)))
for n, t, l, cs in sorted(res, key=lambda x: (-x[0], x[2]))[:top]:
    print(f"{t:06x} calls={n:3d} insns={l:4d} callees=" + ",".join(f"{c:x}" for c in cs))
