# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The variables of a module's small-data area and how L2 code reaches them
# (docs/history/restructure-strategy.md, section 10 step 3).
#
#   statemap.py MODULE TRACE... [--range A B] [--decls]
#
# Reads every declaration in src/game/*/vars.hpp (Var, Table, LibVar,
# PerModule, PerModuleData) and resolves it to its a4 offset in MODULE,
# then the RSR_STATE_TRACE files of a scenario run (RSR_TRACE=1
# tests/run_scenarios.sh). Prints, for a4 offsets in [A, B):
#   - each declaration: its range, the extent the scenarios actually used
#     (arrays), and the other declarations it overlaps;
#   - raw accesses (through a stored pointer, a byte copy or a string
#     function) into declared variables, i.e. where an address escapes;
#   - bytes accessed that no declaration covers.
# --decls: only the declarations, no trace needed.
import glob, os, re, sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MODULES = ("storm", "redstorm", "title", "redstart", "red")
A4 = {"storm": 0x3c474, "redstorm": 0x24b54, "title": 0x2d714, "redstart": 0x3097c, "red": 0x3b174}
LIB_SHIFT = {"storm": (0, 0), "redstorm": (624, 12492), "title": (-5112, -11880),
             "redstart": (19652, -5340), "red": (7324, 21820)}
RAW, BYTES, STRING = -0x40000000, -0x40000000 + 1, -0x40000000 + 2
KIND = {RAW: "pointer", BYTES: "bytes", STRING: "string"}

PRIM = {"be16": 2, "sbe16": 2, "be32": 4, "sbe32": 4, "uint8_t": 1, "int8_t": 1, "char": 1,
        "uint16_t": 2, "int16_t": 2, "uint32_t": 4, "int32_t": 4}


def strip_comments(t):
    return re.sub(r'//[^\n]*', '', t)


def parse_structs(texts):
    sizes = dict(PRIM)
    asserted = {}
    bodies = {}
    for t in texts:
        for m in re.finditer(r'static_assert\(sizeof\((\w+)\) == (0x[0-9a-f]+|\d+)\)', t):
            asserted[m.group(1)] = int(m.group(2), 0)
        for m in re.finditer(r'struct (\w+) \{(.*?)\n\};', strip_comments(t), re.S):
            bodies[m.group(1)] = m.group(2)

    def size_of(ty, depth=0):
        ty = ty.strip().replace("screen::", "")
        if ty.startswith("Ptr<"): return 4
        if ty in asserted: return asserted[ty]
        if ty in sizes: return sizes[ty]
        if ty not in bodies or depth > 10: return None
        total = 0
        for decl in bodies[ty].split(";"):
            decl = decl.strip()
            if not decl or decl.startswith(("static", "template", "using")) or "(" in decl: continue
            m = re.match(r'((?:const )?[\w:]+(?:<[^>]*>+)?)\s+(.*)', decl, re.S)
            if not m: continue
            base = size_of(m.group(1), depth + 1)
            if base is None: return None
            for name in m.group(2).split(","):
                n = 1
                for d in re.findall(r'\[(\w+)\]', name):
                    n *= int(d, 0) if d[0].isdigit() else {"kWalkers": 4}.get(d, 1)
                total += base * n
        sizes[ty] = total
        return total
    return size_of


def declarations(module):
    files = sorted(glob.glob(os.path.join(ROOT, "src", "game", "*", "vars.hpp")))
    files.append(os.path.join(ROOT, "src", "game", "common", "record.hpp"))
    texts = {f: open(f).read() for f in files}
    size_of = parse_structs(texts.values())
    mi = MODULES.index(module)
    decls = []
    for f, t in texts.items():
        group = os.path.basename(os.path.dirname(f))
        for m in re.finditer(r'inline constexpr (Var|Table|LibVar|PerModule|PerModuleData)<(.+?)>\s+([^;]*);',
                             strip_comments(t), re.S):
            kind, ty, rest = m.groups()
            if ty.count("<") > ty.count(">"): ty += ">"
            ty = ty.strip()
            for d in re.finditer(r'(\w+)\s*\{(\{[^}]*\}[^}]*|[^{}]*)\}', rest):
                name, args = d.groups()
                nums = re.findall(r'-?(?:0x[0-9a-f]+|\d+)', args)
                if kind in ("Var", "Table"):
                    # a module group's own variables; the shared groups' are the same in all
                    if group in MODULES and group != module: continue
                    off = int(nums[0], 0)
                elif kind == "LibVar":
                    blk = 0 if "Data" in args else 1
                    off = int(nums[0], 0) + LIB_SHIFT[module][blk]
                elif kind == "PerModule":
                    off = int(nums[mi], 0)
                    if off == 0 and module != "redstart": continue
                else:  # PerModuleData: a tool address
                    a = int(nums[mi], 0)
                    if a == 0: continue
                    off = a - A4[module]
                decls.append({"name": f"{group}::{name}", "kind": kind, "type": ty, "off": off,
                              "size": size_of(ty) or 0})
    return decls


def main():
    args = sys.argv[1:]
    lo, hi = -32768, 32768
    if "--range" in args:
        i = args.index("--range"); lo, hi = int(args[i + 1], 0), int(args[i + 2], 0); del args[i:i + 3]
    only_decls = "--decls" in args
    if only_decls: args.remove("--decls")
    module, traces = args[0], args[1:]
    mi = MODULES.index(module)
    decls = [d for d in declarations(module) if lo <= d["off"] < hi or lo < d["off"] + d["size"] <= hi]
    by_off = defaultdict(list)
    for d in decls: by_off[d["off"]].append(d)

    used = defaultdict(lambda: [None, None])  # label -> [min, max) actually touched
    raw = defaultdict(int)                    # (kind, off, size) -> count
    for t in traces:
        for line in open(t):
            if line.startswith("#"): continue
            m, off, size, label, n = map(int, line.split())
            if m != mi or not (lo <= off < hi): continue
            if label in KIND:
                raw[(KIND[label], off, size)] += n
            else:
                u = used[label]
                u[0] = off if u[0] is None else min(u[0], off)
                u[1] = off + size if u[1] is None else max(u[1], off + size)

    def extent(d):
        u = used.get(d["off"])
        a, b = d["off"], d["off"] + max(d["size"], 1)
        if u and u[0] is not None: a, b = min(a, u[0]), max(b, u[1])
        return a, b

    spans = sorted(((*extent(d), d) for d in decls), key=lambda x: (x[0], x[1]))
    print(f"{module}: {len(decls)} declarations in [{lo}, {hi})")
    for a, b, d in spans:
        others = [e["name"] for a2, b2, e in spans if e is not d and a2 < b and a < b2]
        u = used.get(d["off"])
        tag = "" if u else "  (not reached)"
        print(f"  {a:6d}..{b:6d} {d['name']:<40} {d['kind']}<{d['type']}>{tag}"
              + (f"\n{'':18}overlaps: {', '.join(sorted(set(others)))}" if others else ""))
    if only_decls or not traces: return
    print("raw accesses (pointer, copy, string) into the range:")
    for (kind, off, size), n in sorted(raw.items(), key=lambda x: x[0][1]):
        names = sorted({d["name"] for a, b, d in spans if a < off + size and off < b})
        print(f"  {kind:7} {off:6d}+{size:<5d} x{n:<8d} {', '.join(names) or '(undeclared)'}")


if __name__ == "__main__":
    main()
