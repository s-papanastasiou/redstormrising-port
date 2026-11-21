#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Save and recall a game through the game's own menus (RedStormSave: maps
# to --save DIR). Starts a training mission, opens the Computer Log, saves,
# then recalls the save in a fresh run and checks the module reads it back
# and resumes the mission. --det-timing, like tests/scenarios. Also exercises large pointer moves (JOY0DAT's
# 8-bit counters: big jumps must be fed in hardware-sized steps).
set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "usage: $0 DATA_DIR [rsr-executable]" >&2
  exit 2
fi
repo=$(cd "$(dirname "$0")/.." && pwd)
data=$1
bin=${2:-"$repo/build/rsr"}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
mkdir -p "$tmp/save"

# Title -> START -> 1984 -> identification test -> name "TEST" -> briefing
# screens -> "Your Orders, Sir?" -> Computer Log -> SAVE -> confirm.
cat >"$tmp/save.txt" <<SCRIPT
2450 mouse 0 -75
2470 click
2670 click
2970 click
3150 key T
3160 key E
3170 key S
3180 key T
3190 key Return
3400 click
3800 click
4200 click
4600 click
4950 mouse -85 170
5050 lmb 1
5070 lmb 0
5300 key Space
5500 key Return
5700 click
6600 quit
SCRIPT
"$bin" --ntsc --fast --det-timing --save "$tmp/save" --script "$tmp/save.txt" "$data" >"$tmp/save.log" 2>&1
test -s "$tmp/save/Games" && test -s "$tmp/save/Game0" || {
  cat "$tmp/save.log" >&2; ls -la "$tmp/save" >&2
  echo 'FAIL: the game did not write RedStormSave:Games/Game0' >&2; exit 1; }

# Fresh run: RECALL -> the first saved game -> the identification test ->
# the mission resumes at "Your Orders, Sir?".
cat >"$tmp/recall.txt" <<SCRIPT
2450 mouse 0 -3
2470 click
2800 mouse 0 -60
2950 click
3300 click
3800 shot $tmp/resumed.ppm
3850 quit
SCRIPT
"$bin" --ntsc --fast --det-timing --verbose --save "$tmp/save" --script "$tmp/recall.txt" "$data" >"$tmp/recall.log" 2>&1
grep -q 'Open(RedStormSave:Games, 1005)' "$tmp/recall.log"
grep -q 'Open(RedStormSave:Game0, 1005)' "$tmp/recall.log" || {
  echo 'FAIL: the recalled game was not read' >&2; exit 1; }
test -s "$tmp/resumed.ppm" || { echo 'FAIL: the recall run did not reach its last frame' >&2; exit 1; }
if grep -q 'HLE: unimplemented\|DisplayAlert' "$tmp/recall.log" "$tmp/save.log"; then
  grep 'HLE: unimplemented\|DisplayAlert' "$tmp/recall.log" "$tmp/save.log" >&2
  echo 'FAIL: unimplemented OS call or alert' >&2; exit 1
fi
echo "PASS: saved $(stat -c%s "$tmp/save/Game0")-byte Game0 and recalled it"
