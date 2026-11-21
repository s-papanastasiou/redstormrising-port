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
    return None
done={int(x,16) for x in open(S+"done.txt").read().split()}
storm=parse(S+"storm.s")
mods={m:parse(S+m+".s") for m in ["redstorm","title","redstart","red"]}
idx={}
for m,ins in mods.items():
    d={}
    for i,(a,o,r) in enumerate(ins):
        if i==0 or ins[i-1][1] in ("rts","jmp","bras","braw"):
            sh=shape(ins,i)
            if sh: d.setdefault(sh,[]).append(a)
    idx[m]=d
# entries in storm: after rts
for i,(a,o,r) in enumerate(storm):
    if a in done: continue
    if i==0 or storm[i-1][1] not in ("rts","jmp","bras","braw"): continue
    sh=shape(storm,i)
    if not sh or len(sh)<4: continue
    where={m:idx[m].get(sh) for m in mods}
    n=sum(1 for v in where.values() if v)
    if n>=4:
        print(f"{a:06x} len={len(sh):3d} "+" ".join(f"{m}:{v[0]:x}" for m,v in where.items()))
