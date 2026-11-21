# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Static check of the game code's rules (docs/history/restructure-strategy.md, section 5)
# for the function bodies in src/game (the memory layer in src/game/mem,
# the thunk files and declaration headers are exempt): no CPU state, no raw memory
# access, no calls by address, no numeric addresses.
#   python3 tools/analysis/check_code_rules.py      exit 1 and list the offences
import glob, os, re, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
FORBIDDEN = [
    (r"\bcpu\(\)|\bc\.(d|a)\[", "CPU state"),
    (r"\b(rd|wr)(8|16|32)\(|\bmaddr\(|\bview<", "raw memory access (declare a variable or record)"),
    (r"\b(call_raw|call_lib|push_[wl]|pop|self|spin)\(|\bflags_\w+\(|\bset_[wb]\(", "register-level call or stack/flag code"),
    (r"\b0x[0-9a-fA-F]{4,}[uUlL]*\b|\b\d{4,}[uUlL]*\b", "numeric address or offset (name it in a declarations header)"),
]


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    return [re.sub(r"//.*", "", l) for l in text.split("\n")]


def main():
    bad = 0
    files = [f for f in glob.glob(os.path.join(ROOT, "src", "game", "**", "*.cpp"), recursive=True)
             if os.sep + "mem" + os.sep not in f and not f.endswith("thunks.cpp")]
    for f in sorted(files):
        for n, line in enumerate(strip_comments(open(f).read()), 1):
            if re.match(r"\s*[A-Z][A-Z0-9_]*\(", line):
                continue  # a registration macro: it names the original's address
            for pat, why in FORBIDDEN:
                if re.search(pat, line):
                    print(f"{os.path.relpath(f, ROOT)}:{n}: {why}: {line.strip()}")
                    bad += 1
    print(f"{len(files)} game files checked, {bad} offences")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
