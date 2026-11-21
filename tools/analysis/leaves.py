# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
import os
import re, sys, collections
lines = [l.rstrip("\n") for l in open(sys.argv[1])]
ins = []
for l in lines:
    m = re.match(r"([0-9a-f]{6}): (\S+) ?(.*)", l)
    if m: ins.append((int(m.group(1),16), m.group(2), m.group(3)))
addr_idx = {a:i for i,(a,_,_) in enumerate(ins)}
# resolve jump stubs: "jmp 0xNNNN" at stub address
stub = {a:int(re.match(r"0x([0-9a-f]+)$",r).group(1),16) for a,o,r in ins if o=="jmp" and re.match(r"0x[0-9a-f]+$",r)}
calls = collections.Counter()
for a,o,r in ins:
    if o in ("jsr","bsrw","bsrs","bsr"):
        m = re.search(r"0x([0-9a-f]+)", r)
        if m:
            t = int(m.group(1),16); t = stub.get(t,t); calls[t]+=1
def body(entry):
    i = addr_idx.get(entry)
    if i is None: return None
    out=[]
    while i < len(ins):
        a,o,r = ins[i]; out.append((a,o,r))
        if o in ("rts","rte") : return out
        if o=="jmp": return out
        i+=1
        if len(out)>400: return None
    return None
res=[]
for t,n in calls.items():
    b = body(t)
    if not b: continue
    leaf = not any(o in ("jsr","bsrw","bsrs","bsr","trap") or "0xdff" in r or "0xbfe" in r or "(-" in r and "%a6" in r for a,o,r in b)
    # branch out of body? (bra beyond rts) – crude: check branch targets inside [entry, last]
    lo, hi = b[0][0], b[-1][0]
    inside = True
    for a,o,r in b:
        if o.startswith("b") and not o.startswith("bsr") and o not in ("btst","bclr","bset","bchg"):
            m=re.search(r"0x([0-9a-f]+)",r)
            if m and not (lo<=int(m.group(1),16)<=hi): inside=False
    if leaf and inside: res.append((n,t,len(b)))
for n,t,l in sorted(res, reverse=True)[:40]: print(f"{t:06x} calls={n:3d} insns={l}")
