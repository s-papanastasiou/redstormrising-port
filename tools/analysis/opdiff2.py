# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Operand differences between two copies of a routine in any two modules:
#   python3 tools/analysis/opdiff2.py MOD1 ADDR1 MOD2 ADDR2
# (opdiff.py compares against Storm only.)
import re,sys
S='build/dis/'
def body(m,a):
    out=[];f=0
    for l in open(S+m+'.s'):
        x=re.match(r'([0-9a-f]{6}): (\S+) ?(.*)',l)
        if not x: continue
        if int(x.group(1),16)==a: f=1
        if f:
            out.append((int(x.group(1),16),x.group(2),x.group(3)))
            if x.group(2)=='rts': return out
m1,a1,m2,a2=sys.argv[1],int(sys.argv[2],16),sys.argv[3],int(sys.argv[4],16)
b1,b2=body(m1,a1),body(m2,a2)
print(len(b1),len(b2))
for (x,o1,r1),(y,o2,r2) in zip(b1,b2):
    if o1!=o2 or r1!=r2: print(f"+{x-a1:#x} {o1} {r1} | {o2} {r2}")
