# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# What do the callers of each routine read after it returns?
# (docs/history/restructure-strategy.md, task T1.)
#
#   python3 tools/analysis/liveness.py            report
#   python3 tools/analysis/liveness.py --write    also write src/native/live_after_call.inc
#   python3 tools/analysis/liveness.py --check    exit 1 if that file is out of date
#   python3 tools/analysis/liveness.py MODULE ADDR   explain one routine (call sites)
#
# For every routine (a call target in build/dis/MODULE.s) the tool follows
# the code after each call site and records which of these are read before
# they are overwritten:
#   the condition codes N/Z/V/C, the X flag,
#   d0 and d1 in three parts (bits 0-7, 8-15, 16-31), a0, a1.
# A native replacement only has to get right what some caller reads.
#
# The scan follows both sides of conditional branches. At a call to another
# routine it uses that routine's summary (what it reads on entry, what it
# may leave untouched); at `rts` what is still unread flows to the callers
# of the routine containing the call site; both by fixpoint iteration.
# Copies are followed: `move.l d0,d7` is not a read of d0; whatever is
# read from d7 later is. (Compiled code copies whole registers whose upper
# word it never uses.) A value written to memory counts as read. Calls are
# assumed to preserve d2-d7/a2-a6 and not to read them.
# Results are split in "live" (a read was found) and "maybe" (the scan
# reached something it cannot follow: an OS call that may take the
# register as an argument, an indirect call or jump, data in the code).
# The verifier treats "maybe" as live only for d0, within the declared
# result of a C routine (src/amiga/modules.cpp, verify_native).
#
# Routines never called directly (function pointers, interrupt code) and
# routines whose address is taken are not in the table: the verifier then
# compares everything, as before.
import bisect, os, re, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
DIS = os.environ.get("RSR_DIS", os.path.join(ROOT, "build", "dis"))
OUT = os.path.join(ROOT, "src", "native", "live_after_call.inc")
MODULES = ["storm", "redstorm", "title", "redstart", "red"]
# Data the disassembly decodes as code (start, end): calls found there are
# not calls. Every module's C start-up is followed by "dos.library", which
# decodes as a bsr.s to $20220 (inside the module's main routine).
DATA_IN_CODE = [(0x201a4, 0x201b0)]

# Locations, one bit each: 0 N, 1 X, 2+3n.. the three parts of dn (bits
# 0-7, 8-15, 16-31), 26+n an (a0-a6), 33 Z, 34 V, 35 C. The locations a
# call may change (the flags, d0, d1, a0, a1) are also the "origins"
# reported; N, Z, V and C are reported together as nzvc.
FN, X, FZ, FV, FC = 1, 2, 1 << 33, 1 << 34, 1 << 35
NZVC = FN | FZ | FV | FC
NLOC = 36
COND = {"eq": FZ, "ne": FZ, "cc": FC, "cs": FC, "pl": FN, "mi": FN, "vc": FV, "vs": FV,
        "ge": FN | FV, "lt": FN | FV, "gt": FN | FV | FZ, "le": FN | FV | FZ,
        "hi": FC | FZ, "ls": FC | FZ}


def dreg(n, size):
    """Location bits of data register n covered by an access of this size."""
    b = 1 << (2 + 3 * n)
    return {"b": b, "w": b * 3, "l": b * 7}[size]


def areg(n):
    return 1 << (26 + n) if n < 7 else 0


D0B, D0W, D0H = dreg(0, "b"), dreg(0, "b") << 1, dreg(0, "b") << 2
D1B, D1W, D1H = dreg(1, "b"), dreg(1, "b") << 1, dreg(1, "b") << 2
A0, A1 = areg(0), areg(1)
CC = NZVC | X
D0 = D0B | D0W | D0H
D1 = D1B | D1W | D1H
ARGS = D0 | D1 | A0 | A1
ALL = CC | ARGS                      # what a call may change
SCRATCH = [l for l in range(NLOC) if ALL >> l & 1]
NAMES = [(NZVC, "nzvc"), (X, "x"), (D0B, "d0.b"), (D0W, "d0.w"), (D0H, "d0.hi"),
         (D1B, "d1.b"), (D1W, "d1.w"), (D1H, "d1.hi"), (A0, "a0"), (A1, "a1")]
IDENT = tuple((1 << l) if ALL >> l & 1 else 0 for l in range(NLOC))


def names(mask):
    return " ".join(n for b, n in NAMES if mask & b) or "-"


def out_bits(mask):
    """The table's 10-bit encoding."""
    return sum(1 << i for i, (b, _) in enumerate(NAMES) if mask & b)


def bits_of(mask):
    l = 0
    while mask:
        if mask & 1: yield l
        mask >>= 1; l += 1


REG = re.compile(r"%(d[0-7]|a[0-7]|sp|fp|pc)")


def reg_bits_all(text):
    """Everything an effective address reads to form its address."""
    bits = 0
    for m in re.finditer(r"%(d[0-7]|a[0-7])(?::([wl]))?", text):
        r, sz = m.group(1), m.group(2)
        if r[0] == "d": bits |= dreg(int(r[1]), sz or "l")
        else: bits |= areg(int(r[1]))
    if "%fp" in text: bits |= areg(6)
    return bits


def split_ops(s):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch == "(": depth += 1
        elif ch == ")": depth -= 1
        if ch == "," and depth == 0:
            out.append(cur); cur = ""
        else:
            cur += ch
    if cur: out.append(cur)
    return [o.strip() for o in out]


def operand(op, size):
    """(kind, read bits to address it, bits of the register itself)."""
    m = re.fullmatch(r"%d([0-7])", op)
    if m: return ("d", 0, dreg(int(m.group(1)), size), dreg(int(m.group(1)), "l"))
    m = re.fullmatch(r"%a([0-7])", op)
    if m: return ("a", 0, areg(int(m.group(1))), areg(int(m.group(1))))
    if op == "%fp": return ("a", 0, areg(6), areg(6))
    if op == "%sp": return ("a", 0, 0, 0)
    if op.startswith("#"): return ("imm", 0, 0, 0)
    return ("mem", reg_bits_all(op), 0, 0)


def movem_list(op):
    bits = 0
    for part in op.split("/"):
        m = re.fullmatch(r"%([da])([0-7])(?:-%([da])([0-7]))?", part.replace("%fp", "%a6").replace("%sp", "%a7"))
        if not m: return None
        lo = int(m.group(2)); hi = int(m.group(4)) if m.group(4) else lo
        for n in range(lo, hi + 1):
            bits |= dreg(n, "l") if m.group(1) == "d" else areg(n)
    return bits


BRANCH = re.compile(r"b(eq|ne|ge|lt|gt|le|cc|cs|pl|mi|hi|ls|vc|vs)[sw]$")
SCC = re.compile(r"s(eq|ne|ge|lt|gt|le|cc|cs|pl|mi|hi|ls|vc|vs)$")
BASES = sorted(["movea", "movem", "moveq", "move", "lea", "pea", "clr", "tst", "cmpi", "cmpa",
                "cmp", "addq", "addi", "adda", "add", "subq", "subi", "suba", "sub", "andi", "and",
                "ori", "or", "eori", "eor", "neg", "not", "ext", "swap", "muls", "mulu", "divs",
                "divu", "asl", "asr", "lsl", "lsr", "rol", "ror", "roxl", "roxr", "btst", "bset",
                "bclr", "bchg", "exg", "link", "unlk", "nop", "dbf", "dbra"], key=len, reverse=True)


def base_size(mn):
    for b in BASES:
        if mn == b: return b, None
        if mn.startswith(b) and mn[len(b):] in ("b", "w", "l"): return b, mn[len(b):]
    return None, None


def effect(mn, ops_text):
    """(reads, writes) of a straight-line instruction, or None if unknown.
    Reads happen before writes."""
    base, size = base_size(mn)
    if SCC.match(mn):
        k, addr, bits, _ = operand(ops_text, "b")
        return COND[SCC.match(mn).group(1)] | addr, bits if k == "d" else 0
    if base is None: return None
    ops = split_ops(ops_text)
    if base in ("nop", "unlk", "link"): return 0, 0
    if base == "movem":
        if len(ops) != 2: return None
        l0, l1 = movem_list(ops[0]), movem_list(ops[1])
        if l0 is not None and not ops[0].endswith(")") and "@" not in ops[0]:
            if size == "w":  # stores the low words only
                l0 &= ~sum(dreg(n, "l") & ~dreg(n, "w") for n in range(8))
            return l0 | reg_bits_all(ops[1]), 0
        if l1 is not None: return reg_bits_all(ops[0]), l1
        return None
    if base == "moveq":
        _, _, _, full = operand(ops[1], "l")
        return 0, full | NZVC
    if base in ("lea", "movea"):
        k, addr, bits, _ = operand(ops[0], size or "l")
        _, _, _, full = operand(ops[1], "l")
        return addr | (bits if k in "da" else 0), full
    if base == "pea":
        return reg_bits_all(ops[0]), 0
    if base == "move":
        if "%sr" in ops_text or "%ccr" in ops_text or "%usp" in ops_text: return None
        k0, a0, b0, _ = operand(ops[0], size)
        k1, a1, b1, _ = operand(ops[1], size)
        return a0 | (b0 if k0 in "da" else 0) | a1, (b1 if k1 == "d" else 0) | NZVC
    if base == "clr":
        k, addr, bits, _ = operand(ops[0], size)
        return addr, (bits if k == "d" else 0) | NZVC
    if base == "tst":
        k, addr, bits, _ = operand(ops[0], size)
        return addr | bits, NZVC
    if base in ("cmp", "cmpi", "cmpa"):
        k0, a0, b0, _ = operand(ops[0], size)
        k1, a1, b1, f1 = operand(ops[1], size)
        return a0 | b0 | a1 | (f1 if k1 == "a" else b1), NZVC
    if base in ("add", "addi", "addq", "sub", "subi", "subq", "and", "andi", "or", "ori", "eor",
                "eori", "adda", "suba"):
        k0, a0, b0, _ = operand(ops[0], size)
        k1, a1, b1, f1 = operand(ops[1], size)
        reads = a0 | b0 | a1
        if base in ("sub", "suba", "eor") and ops[0] == ops[1] and k1 in "da" and (size == "l" or k1 == "a"):
            return 0, f1 | (0 if k1 == "a" else NZVC | (X if base == "sub" else 0))  # clears it
        if k1 == "a": return reads | f1, f1  # no flags for address registers
        flags = NZVC | (X if base[:3] in ("add", "sub") else 0)
        return reads | b1, (b1 if k1 == "d" else 0) | flags
    if base in ("neg", "not"):
        k, addr, bits, _ = operand(ops[0], size)
        return addr | bits, (bits if k == "d" else 0) | NZVC | (X if base == "neg" else 0)
    if base == "ext":
        n = int(ops[0][2])
        if size == "w": return dreg(n, "b"), dreg(n, "w") | NZVC
        return dreg(n, "w"), dreg(n, "l") | NZVC
    if base == "swap":
        _, _, _, full = operand(ops[0], "l")
        return full, full | NZVC
    if base in ("muls", "mulu"):
        k0, a0, b0, _ = operand(ops[0], "w")
        n = int(ops[1][2])
        return a0 | b0 | dreg(n, "w"), dreg(n, "l") | NZVC
    if base in ("divs", "divu"):
        k0, a0, b0, _ = operand(ops[0], "w")
        n = int(ops[1][2])
        return a0 | b0 | dreg(n, "l"), dreg(n, "l") | NZVC
    if base in ("asl", "asr", "lsl", "lsr", "rol", "ror", "roxl", "roxr"):
        reads = X if base.startswith("rox") else 0
        sets_x = not base.startswith("ro") or base.startswith("rox")
        if len(ops) == 1:  # memory, by one
            return reads | reg_bits_all(ops[0]), NZVC | (X if sets_x else 0)
        k0, _, b0, _ = operand(ops[0], "b")
        k1, _, b1, _ = operand(ops[1], size)
        # A register count of 0 leaves X alone: X is only killed by an
        # immediate count.
        return reads | (b0 if k0 == "d" else 0) | b1, b1 | NZVC | (X if sets_x and k0 == "imm" else 0)
    if base in ("btst", "bset", "bclr", "bchg"):
        k0, _, b0, _ = operand(ops[0], "b")
        k1, a1, _, f1 = operand(ops[1], "l")
        return (b0 if k0 == "d" else 0) | a1 | f1, FZ  # sets Z only
    if base == "exg":
        _, _, _, f0 = operand(ops[0], "l")
        _, _, _, f1 = operand(ops[1], "l")
        return f0 | f1, 0
    return None


class Module:
    def __init__(self, name):
        self.name = name
        self.ins = []
        for l in open(os.path.join(DIS, name + ".s")):
            m = re.match(r"([0-9a-f]{6}): (\S+) ?(.*)", l.rstrip("\n"))
            if m: self.ins.append((int(m.group(1), 16), m.group(2), m.group(3)))
        # The disassembler leaves some instructions as data words after
        # padding or jump tables: recover rts and `jsr d16(pc)`.
        fixed, k = [], 0
        while k < len(self.ins):
            a, o, r = self.ins[k]
            if o == ".short" and r == "0x4e75":
                fixed.append((a, "rts", ""))
            elif o == ".short" and r == "0x4eba" and k + 1 < len(self.ins) \
                    and self.ins[k + 1][1] == ".short" and self.ins[k + 1][0] == a + 2:
                d = int(self.ins[k + 1][2], 16)
                fixed.append((a, "jsr", f"%pc@(0x{a + 2 + (d - 0x10000 if d & 0x8000 else d):x})"))
                k += 1
            else:
                fixed.append((a, o, r))
            k += 1
        self.ins = fixed
        self.addrs = [a for a, _, _ in self.ins]
        self.idx = {a: i for i, a in enumerate(self.addrs)}
        self.stub = {a: int(r[2:], 16) for a, o, r in self.ins
                     if o == "jmp" and re.fullmatch(r"0x[0-9a-f]+", r)}
        self.sites = []  # (index of the call, target)
        targets = set()
        for i, (a, o, r) in enumerate(self.ins):
            if any(lo <= a < hi for lo, hi in DATA_IN_CODE): continue
            t = self.call_target(o, r)
            if t is not None and t in self.idx:
                self.sites.append((i, t)); targets.add(t)
        self.entries = sorted(targets)
        self.entry_set = set(self.entries)
        self.taken = set()
        for a, o, r in self.ins:
            if o in ("jsr", "jmp") or o.startswith("bsr") or BRANCH.match(o) or o in ("bras", "braw", "dbf"):
                continue
            for m in re.finditer(r"(?<![\w@(])(?:#)?(0x[0-9a-f]+|\d{6,})\b|%pc@\((0x[0-9a-f]+)\)", r):
                v = m.group(1) or m.group(2)
                v = int(v, 16) if v.startswith("0x") else int(v)
                v = self.stub.get(v, v)
                if v in self.entry_set: self.taken.add(v)
        self.reads = {e: 0 for e in self.entries}   # scratch locations read on entry
        self.maybe = {e: 0 for e in self.entries}
        # thru[e][k]: entry locations whose value may be in SCRATCH[k] at rts
        self.thru = {e: tuple(0 for _ in SCRATCH) for e in self.entries}

    def direct(self, o, r):
        m = re.fullmatch(r"(?:%pc@\()?0x([0-9a-f]+)\)?", r)
        if not m: return None
        t = int(m.group(1), 16)
        seen = set()
        while t in self.stub and t not in seen:
            seen.add(t); t = self.stub[t]
        return t

    def call_target(self, o, r):
        if o == "jsr" or o.startswith("bsr"): return self.direct(o, r)
        return None

    def containing(self, addr):
        k = bisect.bisect_right(self.entries, addr) - 1
        return self.entries[k] if k >= 0 else None

    def copy(self, o, r, st):
        """Register to register moves carry what is tracked instead of
        reading it. Returns the new state, or None if `o r` is not one."""
        base, size = base_size(o)
        if base not in ("move", "movea"): return None
        ops = split_ops(r)
        if len(ops) != 2: return None
        m0 = re.fullmatch(r"%([da])([0-7])", ops[0].replace("%fp", "%a6"))
        m1 = re.fullmatch(r"%([da])([0-7])", ops[1].replace("%fp", "%a6"))
        if not m0 or not m1 or m0.group(2) == "7" and m0.group(1) == "a": return None
        st = list(st)
        n0, n1 = int(m0.group(2)), int(m1.group(2))
        nparts = {"b": 1, "w": 2, "l": 3}[size or "l"]
        if m0.group(1) == "d": src = [st[2 + 3 * n0 + k] for k in range(nparts)]
        else: src = [st[26 + n0]] * nparts
        moved = 0
        for v in src: moved |= v
        if m1.group(1) == "d":
            for k in range(nparts): st[2 + 3 * n1 + k] = src[k]
        elif n1 < 7:
            st[26 + n1] = moved  # movea.w sign-extends: all of it derives from the source
        if base == "move":
            for l in bits_of(NZVC): st[l] = moved
        return tuple(st)

    def explore(self, start, state):
        """Follow the code from instruction index `start`. state[l] is the
        set of tracked origins whose value may be in location l. Returns
        (origins read, origins maybe read, state at rts merged)."""
        read = maybe = 0
        at_rts = [0] * NLOC
        seen = set()
        work = [(start, state)]
        self.where = {}  # first instruction found reading each origin (for the explain mode)

        def note(a, before):
            for l in bits_of(read & ~before): self.where.setdefault(l, a)

        def use(st, locs):
            v = 0
            for l in bits_of(locs): v |= st[l]
            return v

        def everything(st):
            v = 0
            for x in st: v |= x
            return v

        noted = [0]
        last = [0]
        while work:
            i, st = work.pop()
            note(last[0], noted[0]); noted[0] = read
            while True:
                note(last[0], noted[0]); noted[0] = read
                last[0] = self.ins[i][0] if i < len(self.ins) else 0
                if read: st = tuple(x & ~read for x in st)
                if not any(st): break
                if i >= len(self.ins):
                    maybe |= everything(st); break
                if (i, st) in seen: break
                seen.add((i, st))
                a, o, r = self.ins[i]
                if o == "rts":
                    for l in range(NLOC): at_rts[l] |= st[l]
                    break
                if o in ("bras", "braw"):
                    t = self.direct(o, r)
                    if t not in self.idx: maybe |= everything(st); break
                    i = self.idx[t]; continue
                if BRANCH.match(o):
                    read |= use(st, COND[BRANCH.match(o).group(1)])
                    t = self.direct(o, r)
                    if t in self.idx: work.append((self.idx[t], st))
                    else: maybe |= everything(st)
                    i += 1; continue
                if o in ("dbf", "dbra"):
                    ops = split_ops(r)
                    read |= use(st, dreg(int(ops[0][2]), "w"))
                    t = self.direct(o, ops[1])
                    if t in self.idx: work.append((self.idx[t], st))
                    i += 1; continue
                if o == "jsr" or o.startswith("bsr") or o == "jmp":
                    t = self.direct(o, r)
                    if t is None:
                        read |= use(st, reg_bits_all(r))
                        if o == "jsr" and ("%fp@(-" in r or "%a6@(-" in r):
                            # An OS call: which registers are arguments is
                            # not known here; it preserves d2-d7/a2-a6.
                            maybe |= use(st, ARGS)
                            st = tuple(0 if ALL >> l & 1 else x for l, x in enumerate(st))
                            i += 1; continue
                        maybe |= everything(st)  # indirect call / jump table
                        break
                    if o == "jmp" and t in self.idx and t not in self.entry_set:
                        i = self.idx[t]; continue  # a jump inside the routine
                    if t not in self.entry_set:
                        maybe |= everything(st); break
                    read |= use(st, self.reads[t])
                    maybe |= use(st, self.maybe[t])
                    new = list(st)
                    for k, l in enumerate(SCRATCH): new[l] = use(st, self.thru[t][k])
                    st = tuple(new)
                    if o == "jmp":  # tail call
                        for l in range(NLOC): at_rts[l] |= st[l]
                        break
                    i += 1; continue
                c = self.copy(o, r, st)
                if c is not None:
                    st = c; i += 1; continue
                e = effect(o, r)
                if e is None:
                    maybe |= everything(st); break
                rd, wr = e
                read |= use(st, rd)
                st = tuple(0 if wr >> l & 1 else x for l, x in enumerate(st))
                i += 1
        note(last[0], noted[0])
        return read, maybe, at_rts

    def summarise(self):
        changed = True
        while changed:
            changed = False
            for e in self.entries:
                rd, mb, at = self.explore(self.idx[e], IDENT)
                th = tuple(at[l] | self.thru[e][k] for k, l in enumerate(SCRATCH))
                if rd | self.reads[e] != self.reads[e] or mb | self.maybe[e] != self.maybe[e] \
                        or th != self.thru[e]:
                    self.reads[e] |= rd; self.maybe[e] |= mb; self.thru[e] = th
                    changed = True

    def after_calls(self):
        """live/maybe after return, per routine; and the per-site detail."""
        live = {e: 0 for e in self.entries}
        maybe = {e: 0 for e in self.entries}
        deps = []    # (callee, containing routine or None, state reaching its rts)
        detail = {e: [] for e in self.entries}
        for i, t in self.sites:
            rd, mb, at = self.explore(i + 1, IDENT)
            live[t] |= rd; maybe[t] |= mb
            deps.append((t, self.containing(self.ins[i][0]), at))
            detail[t].append((self.ins[i][0], rd, mb, at, dict(self.where)))
        # Tail calls: `jmp routine` hands the callee's results to the
        # callers of the routine containing the jump.
        for a, o, r in self.ins:
            if o == "jmp" and a not in self.stub:
                t = self.direct(o, r)
                if t in self.entry_set:
                    deps.append((t, self.containing(a), list(IDENT)))
                    detail[t].append((a, 0, 0, list(IDENT), {}))
        changed = True
        while changed:
            changed = False
            for t, c, at in deps:
                add_l = add_m = 0
                for l in SCRATCH:
                    if not at[l]: continue
                    if c is None or c in self.taken: add_m |= at[l]
                    else:
                        if live[c] >> l & 1: add_l |= at[l]
                        if maybe[c] >> l & 1: add_m |= at[l]
                if add_l | live[t] != live[t] or add_m | maybe[t] != maybe[t]:
                    live[t] |= add_l; maybe[t] |= add_m; changed = True
        return live, maybe, detail


def analyse():
    res = {}
    for name in MODULES:
        m = Module(name)
        m.summarise()
        live, maybe, detail = m.after_calls()
        res[name] = (m, live, maybe, detail)
    return res


def table(res):
    lines = ["// Generated by tools/analysis/liveness.py --write. Do not edit.",
             "// What callers read after each routine returns: {module, address, live, maybe}.",
             "// Bits: 1 nzvc, 2 x, 4 d0.b, 8 d0 bits 8-15, 16 d0 high word, 32 d1.b, 64 d1",
             "// bits 8-15, 128 d1 high word, 256 a0, 512 a1. Routines without direct",
             "// callers or whose address is taken are left out (compare everything)."]
    for name in MODULES:
        m, live, maybe, _ = res[name]
        for e in m.entries:
            if e in m.taken: continue
            lines.append(f'{{"{name}", 0x{e:06x}, 0x{out_bits(live[e]):03x}, '
                         f'0x{out_bits(maybe[e] & ~live[e]):03x}}},')
    return "\n".join(lines) + "\n"


def main():
    args = sys.argv[1:]
    res = analyse()
    if len(args) == 2 and not args[0].startswith("--"):
        m, live, maybe, detail = res[args[0]]
        t = int(args[1], 16)
        t = m.stub.get(t, t)
        print(f"{args[0]} {t:06x}: live {names(live[t])}; maybe {names(maybe[t] & ~live[t])}"
              + ("; ADDRESS TAKEN" if t in m.taken else ""))
        kept = 0
        for k, l in enumerate(SCRATCH):
            if m.thru[t][k] >> l & 1: kept |= 1 << l
        print(f"  on entry reads {names(m.reads[t])}; may leave unwritten {names(kept)}")
        for a, rd, mb, at, where in detail[t]:
            out = 0
            for l in SCRATCH: out |= at[l]
            print(f"  site {a:06x} (in {m.containing(a) or 0:06x}): reads {names(rd)}; maybe {names(mb)};"
                  f" to its callers {names(out)}")
            for l, w in sorted(where.items()):
                print(f"      {names(1 << l)} at {w:06x}")
        return 0
    text = table(res)
    if "--check" in args:
        if not os.path.exists(OUT) or open(OUT).read() != text:
            print("live_after_call.inc is out of date: run tools/analysis/liveness.py --write")
            return 1
        return 0
    if "--write" in args:
        open(OUT, "w").write(text)
    total = cc = ccm = xl = d1a = taken = 0
    for name in MODULES:
        m, live, maybe, _ = res[name]
        for e in m.entries:
            if e in m.taken: taken += 1; continue
            total += 1
            if live[e] & NZVC: cc += 1
            elif maybe[e] & NZVC: ccm += 1
            if (live[e] | maybe[e]) & X: xl += 1
            if live[e] & (D1 | A0 | A1): d1a += 1
        flagged = [(e, live[e], maybe[e]) for e in m.entries
                   if e not in m.taken and (live[e] & (CC | D1 | A0 | A1) or maybe[e] & CC)]
        for e, l, mb in flagged:
            print(f"{name:9s} {e:06x} live {names(l & ~D0)}; maybe {names(mb & ~l & (CC))}")
    print(f"{total} routines with direct callers; {taken} with their address taken (not in the table)")
    print(f"callers read the condition codes of {cc} (maybe of {ccm} more), X of {xl}, d1/a0/a1 of {d1a}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
