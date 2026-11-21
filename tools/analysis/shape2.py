# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
import os
import re, sys
S=os.environ.get("RSR_DIS", "build/dis/")
def parse(path):
    ins=[]
    for l in open(path):
        m=re.match(r"([0-9a-f]{6}): (\S+) ?(.*)",l)
        if m: ins.append((int(m.group(1),16),m.group(2),m.group(3).strip()))
    return ins
def shape(ins, i):
    out=[]
    while i < len(ins) and len(out) < 500:
        a,o,r=ins[i]
        r2=re.sub(r"0x[0-9a-f]+","A",r); r2=re.sub(r"-?\d+","N",r2)
        out.append(o+" "+r2)
        if o=="rts": return tuple(out)
        i+=1
src=sys.argv[1]; addrs=[int(x,16) for x in sys.argv[2:]]
base=parse(S+src+".s"); bidx={a:i for i,(a,_,_) in enumerate(base)}
mods={m:parse(S+m+".s") for m in ["storm","redstorm","title","redstart","red"] if m!=src}
for a in addrs:
    sh=shape(base,bidx[a]); hits=[]
    for m,ins in mods.items():
        for i,(b,o,r) in enumerate(ins):
            if i>0 and ins[i-1][1] not in ("rts","jmp","bras","braw"): continue
            if shape(ins,i)==sh: hits.append(f"{m}:{b:x}")
    print(f"{a:x} len={len(sh)} {' '.join(hits)}")
