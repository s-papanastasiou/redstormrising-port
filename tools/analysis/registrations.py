# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Where each C++ function was in the original, for the analysis tools:
# {(module, address): (name, 2)} from ORIGINAL_IN, ORIGINAL, AUDIO_ORIGINAL,
# ORIGINAL_INSIDE and the sound effects table.
import glob, os, re

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MODULE_ORDER = ("storm", "redstorm", "title", "redstart", "red")


def registrations():
    reg = {}
    for d in ("native", "game"):
        for f in glob.glob(os.path.join(ROOT, "src", d, "**", "*.cpp"), recursive=True):
            text = open(f).read()
            for m in re.finditer(r'ORIGINAL_IN\(\s*"(\w+)",\s*0x([0-9a-f]+),\s*(\w+)', text):
                reg[(m.group(1), int(m.group(2), 16))] = (m.group(3), 2)
            # AUDIO_ORIGINAL(fn, storm, title, redstart, red)
            for m in re.finditer(r'AUDIO_ORIGINAL\((\w+), 0x([0-9a-f]+), 0x([0-9a-f]+), 0x([0-9a-f]+), 0x([0-9a-f]+)\)', text):
                for mod, a in zip(("storm", "title", "redstart", "red"), m.groups()[1:]):
                    reg[(mod, int(a, 16))] = (m.group(1), 2)
            # ORIGINAL(fn, storm, redstorm, title, redstart, red), 0 where absent
            for m in re.finditer(r'ORIGINAL\((\w+),\s*((?:0x[0-9a-f]+|0)(?:,\s*(?:0x[0-9a-f]+|0)){4})\)', text):
                for mod, a in zip(MODULE_ORDER, re.split(r',\s*', m.group(2))):
                    if int(a, 16): reg[(mod, int(a, 16))] = (m.group(1), 2)
            # ORIGINAL_INSIDE(name, storm, redstorm, title, redstart, red): done inside another function
            for m in re.finditer(r'ORIGINAL_INSIDE\((\w+),\s*((?:0x[0-9a-f]+|0)(?:,\s*(?:0x[0-9a-f]+|0)){4})\)', text):
                for mod, a in zip(MODULE_ORDER, re.split(r',\s*', m.group(2))):
                    if int(a, 16): reg[(mod, int(a, 16))] = (m.group(1), 2)
            # the effects table (src/game/audio/vars.hpp kEffects, at kEffectBlock)
            if f.endswith(os.path.join("audio", "audio.cpp")):
                hdr = open(os.path.join(os.path.dirname(f), "vars.hpp")).read()
                offs = [int(x, 16) for x in re.findall(r'\{(0x[0-9a-f]{3}), [012], ', hdr)]
                blocks = re.search(r'kEffectBlock\[\] = \{([^}]*)\}', hdr).group(1)
                for mod, b in zip(("storm", "title", "redstart", "red"), re.findall(r'0x([0-9a-f]+)', blocks)):
                    for o in offs: reg[(mod, int(b, 16) + o)] = ("sound_effect", 2)
    return reg
