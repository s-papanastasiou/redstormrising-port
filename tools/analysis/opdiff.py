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
def body(ins, a):
    i=[k for k,(b,_,_) in enumerate(ins) if b==a][0]; out=[]
    while True:
        out.append(ins[i]); 
        if ins[i][1]=="rts": return out
        i+=1
storm=parse(S+"storm.s")
saddr=int(sys.argv[1],16)
sb=body(storm,saddr)
for spec in sys.argv[2:]:
    m,a=spec.split(":"); mb=body(parse(S+m+".s"),int(a,16))
    diffs=[]
    for (x,o,r),(y,o2,r2) in zip(sb,mb):
        n1=re.findall(r"0x[0-9a-f]+|-?\d+",r); n2=re.findall(r"0x[0-9a-f]+|-?\d+",r2)
        for p,q in zip(n1,n2):
            if p!=q:
                # relative: branch/jsr pc targets -> express as offset from routine start
                if p.startswith("0x") and q.startswith("0x"):
                    dp=int(p,16)-saddr; dq=int(q,16)-int(a,16)
                    if dp==dq: continue
                    diffs.append(f"{x-saddr:+#x}:{p}->{q}")
                else: diffs.append(f"{x-saddr:+#x}:{p}->{q}")
    print(m, a, " ".join(diffs) if diffs else "(same up to relocation/relative)")
