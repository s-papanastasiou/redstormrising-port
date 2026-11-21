#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
"""List the AmigaOS library calls an executable makes.

    python3 tools/oscalls.py --fd FD_DIR [--sites] EXE...

Loads each hunk executable with fa18-interceptor-port's loader (relocated at
the same LOAD_BASE as there, *without* fa18's patches), disassembles it with
m68k objdump (OBJDUMP, default m68k-linux-gnu-objdump) and resolves every
`jsr/jmp -N(a6)`:

- a6 loaded from `0x4` is exec;
- a6 loaded from a variable that received `OpenLibrary`'s d0 is the library
  whose name string was in a1 (lea absolute / pc-relative / a4-relative);
- function names come from the NDK .fd files in FD_DIR (exec_lib.fd, ...).

Heuristic, linear scan: a6's source is the last `move.l <src>,a6` before the
call since the last rts/jmp. Unresolved calls are reported, not guessed.
"""
import argparse
import collections
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
FA18_TOOLS = os.environ.get(
    "FA18_TOOLS", os.path.join(HERE, "..", "..", "fa18-interceptor-port", "tools"))
sys.path.insert(0, FA18_TOOLS)
import loadseg  # noqa: E402

# diskfont.library is not in the amitools fd set; its documented layout.
EXTRA = {"diskfont.library": {30: "OpenDiskFont", 36: "AvailFonts",
                              42: "NewFontContents", 48: "DisposeFontContents"}}


def parse_fd(path):
    lvos, bias, base = {}, 30, None
    for line in open(path, encoding="latin-1"):
        line = line.strip()
        if line.startswith("##base"):
            base = line.split()[1]
        elif line.startswith("##bias"):
            bias = int(line.split()[1])
        elif line.startswith("##") or line.startswith("*") or not line:
            continue
        else:
            m = re.match(r"([A-Za-z0-9_]+)\(", line)
            if m:
                lvos[bias] = m.group(1)
                bias += 6
    return lvos


def load_fds(fd_dir):
    libs = {}
    for f in os.listdir(fd_dir):
        m = re.match(r"(.+)_lib\.fd$", f)
        if m:
            libs[m.group(1) + ".library"] = parse_fd(os.path.join(fd_dir, f))
    libs.update(EXTRA)
    return libs


LINE = re.compile(r"^ *([0-9a-f]+):\t[0-9a-f ]+\t(\S+)\s*(.*)$")


def disassemble(img, base):
    objdump = os.environ.get("OBJDUMP", "m68k-linux-gnu-objdump")
    with tempfile.NamedTemporaryFile(suffix=".bin") as tf:
        tf.write(img)
        tf.flush()
        out = subprocess.run([objdump, "-b", "binary", "-m", "m68k:68000", "-D",
                              f"--adjust-vma={base:#x}", tf.name],
                             capture_output=True, text=True, check=True).stdout
    insns = []
    for line in out.splitlines():
        m = LINE.match(line)
        if m:
            insns.append((int(m.group(1), 16), m.group(2), m.group(3)))
    return insns


def cstr(img, base, addr):
    off = addr - base
    if off < 0 or off >= len(img):
        return None
    end = img.find(b"\0", off)
    s = img[off:end if end >= 0 else off + 64]
    return s.decode("latin-1") if s and all(32 <= c < 127 for c in s) else None


def analyse(path, libs):
    hunks, addrs, img = loadseg.load(path, patched=False)
    base = loadseg.LOAD_BASE
    code = [(a, a + h.alloc_size) for h, a in zip(hunks, addrs) if h.kind == "CODE"]
    insns = [i for i in disassemble(img, base) if any(lo <= i[0] < hi for lo, hi in code)]
    a4 = None
    for _, op, args in insns:  # small-data base (Aztec/Lattice: lea DATA,a4)
        m = re.match(r"0x([0-9a-f]+),%a4$", args)
        if op == "lea" and m:
            a4 = int(m.group(1), 16)
            break

    def string_at(src):
        m = re.match(r"%pc@\(0x([0-9a-f]+)\)$", src)
        if m:
            return cstr(img, base, int(m.group(1), 16))
        m = re.match(r"0x([0-9a-f]+)$", src)
        if m:
            return cstr(img, base, int(m.group(1), 16))
        m = re.match(r"%a4@\((-?\d+)\)$", src)
        if m and a4 is not None:
            return cstr(img, base, a4 + int(m.group(1)))
        return None

    def canon(src):  # a4-relative and absolute forms of the same variable
        m = re.match(r"%a4@\((-?\d+)\)$", src or "")
        if m and a4 is not None:
            return f"0x{a4 + int(m.group(1)):x}"
        return src

    # Pass 1: variables that receive OpenLibrary/OldOpenLibrary results.
    var_lib = {"0x4": "exec.library"}
    a1_src = a6_src = None
    for idx, (addr, op, args) in enumerate(insns):
        if op == "lea" and args.endswith(",%a1"):
            a1_src = args[:-4]
        if op in ("moveal", "movel") and args.endswith(",%fp"):
            a6_src = args[:-4]
        m = re.match(r"%fp@\(-(\d+)\)$", args)
        if op in ("jsr", "jmp") and m and int(m.group(1)) in (408, 552) and a6_src == "0x4":
            name = string_at(a1_src) if a1_src else None
            for _, op2, args2 in insns[idx + 1: idx + 6]:
                if op2 == "movel" and args2.startswith("%d0,"):
                    if name:
                        var_lib[canon(args2[4:])] = name
                    break
        if op in ("rts", "jmp") or (op.startswith("bra") and not op.startswith("bras")):
            a6_src = None

    # Pass 2: resolve every library call.
    calls = collections.defaultdict(list)
    unresolved = collections.defaultdict(list)
    a6_src = None
    for addr, op, args in insns:
        if op in ("moveal", "movel") and args.endswith(",%fp"):
            a6_src = canon(args[:-4])
        m = re.match(r"%fp@\(-(\d+)\)$", args)
        if op in ("jsr", "jmp") and m:
            lvo = int(m.group(1))
            lib = var_lib.get(a6_src)
            if lib:
                name = libs.get(lib, {}).get(lvo, "?")
                calls[(lib, lvo, name)].append(addr)
            else:
                unresolved[(a6_src, lvo)].append(addr)
        elif op in ("jsr", "bsrw", "bsrs", "bsrl", "bsr"):
            a6_src = None  # a subroutine may leave a different base in a6
        if op == "rts":
            a6_src = None
    return var_lib, calls, unresolved


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fd", required=True, help="directory with NDK *_lib.fd files")
    ap.add_argument("--sites", action="store_true", help="list call addresses")
    ap.add_argument("exe", nargs="+")
    a = ap.parse_args()
    libs = load_fds(a.fd)
    for path in a.exe:
        var_lib, calls, unresolved = analyse(path, libs)
        print(f"== {os.path.basename(path)}")
        print("   library bases: " + ", ".join(f"{v}={k}" for k, v in sorted(var_lib.items())))
        for (lib, lvo, name), sites in sorted(calls.items()):
            where = " " + " ".join(f"{s:x}" for s in sites) if a.sites else ""
            print(f"   {lib:20} -{lvo:<4} {name:24} x{len(sites)}{where}")
        for (src, lvo), sites in sorted(unresolved.items(), key=lambda kv: str(kv[0])):
            print(f"   unresolved a6={src} -{lvo} x{len(sites)} at {sites[0]:x}")


if __name__ == "__main__":
    main()
