# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
"""The keys of Red Storm Rising (this port) as a one-page cheat sheet.

    python3 tools/docs/keys_cheatsheet.py docs/keys.pdf     (needs reportlab)

The keys are those the game's code handles (src/game/storm/keys.cpp,
src/game/input/, src/game/red/), named as in the game's manual where it
names them; the PC keys are the front end's mapping (src/main.cpp).
"""
import sys

from reportlab.lib import colors
from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.platypus import (BaseDocTemplate, Frame, PageTemplate, Paragraph, Spacer, Table,
                                TableStyle, KeepTogether)

NAVY = colors.HexColor("#1b2a41")
STEEL = colors.HexColor("#3c5a78")
PALE = colors.HexColor("#eef2f6")
KEYFILL = colors.HexColor("#ffffff")
RULE = colors.HexColor("#c5d0db")

SECTIONS = [
    # (column, title, rows); a row: (keys, action); keys "A|B" are alternatives
    (0, "Displays", [
        ("F1", "Tactical display"),
        ("F2", "Weapon control display (a contact selected)"),
        ("F3", "Torpedo defence display"),
        ("F4", "Periscope view"),
        ("F5", "Acoustic signatures (sonar library)"),
        ("F6", "Compare sonar (sonar analysis)"),
        ("F7", "Ship database (identification)"),
        ("F8", "Sea conditions (sonar)"),
        ("Z|X", "Zoom in | out"),
        ("Alt+Z", "Redraw the display"),
    ]),
    (0, "Panel pages", [
        ("C", "Contacts: next contact"),
        ("V", "Weapons loadout: tubes, again for the reserves"),
        ("B", "Damage report"),
        ("N", "Torpedo control: next running torpedo"),
        ("M", "Threats: enemy weapons"),
        ("Help", "Tactical analysis: the computer's suggestion"),
    ]),
    (0, "Helm", [
        ("+|-", "Speed up | down (also = and _)"),
        (",|.", "Rudder left | right (also < and >)"),
        ("/", "Straight and level: cancel course and depth orders"),
        ("F9", "Order a depth: type 10-999, Return"),
        ("F10", "Order a course: type 0-360, Return"),
        ("8", "Active sonar on | off"),
        ("9", "Radar on | off (periscope depth or shallower)"),
        ("0", "Rig for silent running"),
    ]),
    (1, "Weapons", [
        ("1", "Noisemaker"),
        ("2", "Decoy"),
        ("3", "Stinger (at periscope depth; from the store)"),
        ("4", "Mk 48 wire-guided torpedo"),
        ("5", "Sea Lance"),
        ("6", "Harpoon"),
        ("7", "Tomahawk (TASM when carried)"),
        ("Shift+4..7", "Load a tube with that weapon (all full: which tube to unload)"),
        ("Mouse|Arrows", "Aim (the torpedo's PAP point); button, Space or Return fires"),
        ("Esc", "Cancel aiming / unselect"),
    ]),
    (1, "Wire-guided torpedo", [
        ("Shift+F1", "Run shallow"),
        ("Shift+F2", "Run deep"),
        ("Shift+F3", "Search to the left"),
        ("Shift+F4", "Search to the right"),
        ("Shift+F5", "Go active now"),
        ("Arrows", "Move the PAP point"),
        ("Shift+F6", "Drop the torpedo (cut its wire)"),
        ("Shift+F8", "Map overlay on | off (F1, F2, F3)"),
    ]),
    (1, "In the displays", [
        ("Shift+A..V", "Library (F5): compare a class; ship database (F7): its data"),
        ("Return|Backspace", "Library (F5): make the ID positive"),
        ("Arrows", "Periscope (F4): rotate the scope"),
        ("Backspace", "Periscope (F4): identify the ship in view"),
        ("C, F4", "Periscope: the next contact, the scope turned to it"),
        ("0..9", "Number entry: digits; Backspace deletes, Esc cancels"),
        ("Y|N", "The crew's questions: go on | end the battle"),
    ]),
    (2, "Battle control", [
        ("Alt+P", "Pause | resume"),
        ("Alt+T", "Accelerated | real time"),
        ("Alt+A", "Action pictures on | off"),
        ("Alt+V", "Sound on | off"),
        ("Alt+R", "Replay the battle so far"),
        ("Alt+E", "After the battle: no replay"),
    ]),
    (2, "Strategic map", [
        ("(none)", "Drift (0-5 knots)"),
        ("Right button", "Cruise (12-18 knots) towards the pointer, while held"),
        ("Left button", "Flank speed (25+ knots; it cavitates: loud) towards the pointer, while held"),
        ("Arrows", "Cruise that way, while held (moving the mouse alone does nothing)"),
        ("Shift+Arrows", "Flank speed that way, while held"),
        ("Space|Return", "Pause: the control room (orders, log and save, loadout, battle stations)"),
        ("Alt+P", "The same (Esc does not pause)"),
    ]),
    (2, "Menus", [
        ("Mouse|Arrows", "Choose an entry; button or Return selects"),
        ("Letters", "Typing a name"),
    ]),
    (2, "Anywhere", [
        ("Alt+Q", "Quit the game"),
        ("F11", "Fullscreen on | off"),
        ("F12", "Capture | release the mouse"),
    ]),
]

key_style = ParagraphStyle("key", fontName="Helvetica-Bold", fontSize=8.0, leading=9.4, textColor=NAVY)
act_style = ParagraphStyle("act", fontName="Helvetica", fontSize=8.0, leading=9.4, textColor=colors.black)
head_style = ParagraphStyle("head", fontName="Helvetica-Bold", fontSize=10, leading=12, textColor=colors.white)


def keycaps(keys):
    # "A|B" alternatives, "Shift+F1" combinations: each key in brackets-like caps
    alts = keys.split("|")
    out = []
    for a in alts:
        parts = a.split("+") if a not in ("+",) else ["+"]
        if a == "+": parts = ["+"]
        out.append(" + ".join(f'<font backColor="#e4e9ef">&nbsp;{p}&nbsp;</font>' for p in parts))
    return '<font color="#7b8794"> / </font>'.join(out)


def section(title, rows, width):
    data = [[Paragraph(title.upper(), head_style), ""]]
    for keys, action in rows:
        data.append([Paragraph(keycaps(keys), key_style), Paragraph(action.replace("|", "/"), act_style)])
    t = Table(data, colWidths=[width * 0.40, width * 0.60])
    style = [
        ("SPAN", (0, 0), (1, 0)),
        ("BACKGROUND", (0, 0), (1, 0), STEEL),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("TOPPADDING", (0, 0), (-1, -1), 1.8),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 1.8),
        ("LEFTPADDING", (0, 0), (-1, -1), 3),
        ("RIGHTPADDING", (0, 0), (-1, -1), 3),
        ("LINEBELOW", (0, 1), (-1, -2), 0.3, RULE),
    ]
    for r in range(2, len(data), 2):
        style.append(("BACKGROUND", (0, r), (1, r), PALE))
    t.setStyle(TableStyle(style))
    return KeepTogether([t, Spacer(1, 3.4 * mm)])


def page_decor(canvas, doc):
    w, h = landscape(A4)
    canvas.saveState()
    canvas.setFillColor(NAVY)
    canvas.rect(0, h - 17 * mm, w, 17 * mm, stroke=0, fill=1)
    canvas.setFillColor(colors.white)
    canvas.setFont("Helvetica-Bold", 17)
    canvas.drawString(12 * mm, h - 11 * mm, "RED STORM RISING")
    canvas.setFont("Helvetica", 10)
    canvas.drawString(76 * mm, h - 11 * mm, "Keys at a glance")
    canvas.setFont("Helvetica", 7.5)
    canvas.drawRightString(w - 12 * mm, h - 9 * mm, "PC keys for the Amiga's: Help is Home, Alt is either Alt.")
    canvas.drawRightString(w - 12 * mm, h - 12.6 * mm, "Commands are lower-case letters; Shift+letter picks a ship class.")
    canvas.setFillColor(colors.HexColor("#7b8794"))
    canvas.setFont("Helvetica", 6.5)
    canvas.drawString(12 * mm, 6 * mm, "Red Storm Rising port: the keys the game's code handles (src/game/storm/keys.cpp, "
                      "src/game/input, src/game/red). Generated by tools/docs/keys_cheatsheet.py.")
    canvas.restoreState()


def main(out):
    w, h = landscape(A4)
    margin, gap = 12 * mm, 6 * mm
    col_w = (w - 2 * margin - 2 * gap) / 3
    top, bottom = h - 22 * mm, 11 * mm
    frames = [Frame(margin + i * (col_w + gap), bottom, col_w, top - bottom, leftPadding=0, rightPadding=0,
                    topPadding=0, bottomPadding=0, id=f"c{i}") for i in range(3)]
    doc = BaseDocTemplate(out, pagesize=landscape(A4), title="Red Storm Rising: keys",
                          author="Red Storm Rising port", subject="Keyboard cheat sheet")
    doc.addPageTemplates([PageTemplate(id="sheet", frames=frames, onPage=page_decor)])
    story = []
    from reportlab.platypus import FrameBreak
    for col in range(3):
        if col: story.append(FrameBreak())
        for c, title, rows in SECTIONS:
            if c == col: story.append(section(title, rows, col_w))
    doc.build(story)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "docs/keys.pdf")
