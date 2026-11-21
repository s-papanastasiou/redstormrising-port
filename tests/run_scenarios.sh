#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Run every scripted scenario (tests/scenarios/*.txt) headless in parallel,
# as an NTSC Amiga (the scripts' frame numbers were taken on one).
#   tests/run_scenarios.sh OUTDIR [rsr options...]
# Each scenario writes OUTDIR/<name>.log and a screenshot OUTDIR/<name>.ppm
# of the frame before its final "quit". Game data: $RSR_DATA, else the
# WHDLoad data/ directory under extracted/.
# RSR_TRACE=1: also OUTDIR/<name>.trace (L2 accesses to small data, for
# tools/analysis/statemap.py).
set -euo pipefail
out=$1; shift
root=$(cd "$(dirname "$0")/.." && pwd)
data=${RSR_DATA:-$root/extracted/RedStormRising/data}
bin=${RSR_BIN:-$root/build/rsr}
[ -d "$data" ] || { echo "no game data at $data (set RSR_DATA)" >&2; exit 1; }
mkdir -p "$out"
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
pids=()
for scenario in "$root"/tests/scenarios/*.txt; do
  name=$(basename "$scenario" .txt)
  last=$(awk '$2 == "quit" {q = $1} END {print q + 0}' "$scenario")
  save=$(mktemp -d)
  (
    # A hung run never reaches its "quit": kill it after 10 minutes.
    [ -z "${RSR_TRACE:-}" ] || export RSR_STATE_TRACE="$out/$name.trace"
    timeout -s KILL 600 "$bin" --ntsc --fast --no-present --save "$save" --script "$scenario" \
      --screenshot $((last - 1)) "$out/$name.ppm" "$@" "$data" > "$out/$name.log" 2>&1 || true
    rm -rf -- "$save"
  ) &
  pids+=($!)
done
for p in "${pids[@]}"; do wait "$p"; done
