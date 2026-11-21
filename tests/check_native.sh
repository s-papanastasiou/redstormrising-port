#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The gate: must pass before every commit.
#   1. the build and the code rules (tools/analysis/check_code_rules.py)
#   2. determinism: two runs of every scenario give the same hashes
#   3. no access to state that moved into C++ through the emulated memory,
#      no constant data changed, no custom-chip access (the trace run)
#   4. no index outside a native array (a -fsanitize=bounds build)
#   5. the picture and sound equal the recordings (tests/reference)
#   6. a game saved and recalled (tests/check_save_load.sh)
# All runs use --det-timing.
#   tests/check_native.sh [WORKDIR]
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
w=${1:-$(mktemp -d)}
mkdir -p "$w"
# --reference-terrain: Storm's sea bed (its own code read as terrain) as
# the recordings had it, wherever Storm is loaded now (item 5g): moving
# memory around changes the battles' terrain and nothing else.
opts=(--det-timing --ram-hash 50 --reference-terrain)

echo "== build"
if ! cmake --build "$root/build" > "$w/build.txt" 2>&1; then
  grep -E "error" "$w/build.txt" | head -20; echo "FAIL: build failed"; exit 1
fi

echo "== code rules (static)"
if ! python3 "$root/tools/analysis/check_code_rules.py" > "$w/rules.txt"; then cat "$w/rules.txt"; echo "FAIL: code rules"; exit 1; fi
echo "   $(tail -1 "$w/rules.txt")"

echo "== determinism (native on, twice)"
RSR_TRACE=1 bash "$here/run_scenarios.sh" "$w/on" "${opts[@]}" --av-hash 60
bash "$here/run_scenarios.sh" "$w/on2" "${opts[@]}" --av-hash 60
if ! bash "$here/compare_runs.sh" "$w/on" "$w/on2" > "$w/determinism.txt"; then
  cat "$w/determinism.txt"; echo "FAIL: runs are not deterministic"; exit 1
fi
echo "   $(grep -c '^same' "$w/determinism.txt") scenarios deterministic"

# Section 10 step 3: state that moved into C++ is not reached in the
# emulated memory any more (the first run traces L2's memory accesses).
# (Each check collects its lines first: with pipefail, "grep | head | grep ."
# is false when head stops reading early, and a failure went unnoticed.)
grep -h "STATE MOVED" "$w"/on/*.log | sort | uniq -c > "$w/moved.txt" || true
if [ -s "$w/moved.txt" ]; then
  head -20 "$w/moved.txt"
  echo "FAIL: moved state reached in emulated memory"; exit 1
fi
grep -h "^\[io\]" "$w"/on/*.log | sort | uniq -c > "$w/io.txt" || true
if [ -s "$w/io.txt" ]; then
  head -20 "$w/io.txt"
  echo "FAIL: an access outside RAM (no custom chip registers since item 4d)"; exit 1
fi
grep -h "CONST CHANGED" "$w"/on/*.log | sort | uniq -c > "$w/const.txt" || true
if [ -s "$w/const.txt" ]; then
  head -20 "$w/const.txt"
  echo "FAIL: constant data changed in emulated memory (item 3f)"; exit 1
fi

echo "== array bounds (a build with -fsanitize=bounds: native state indexed outside a table)"
# The original reads and writes past its tables (place -1, a text page
# column past the page); a native array does not give the neighbouring
# bytes, so every such access must be handled by hand (section 10 step 3).
bounds="$root/build-bounds"
if [ ! -f "$bounds/build.ninja" ] && ! cmake -S "$root" -B "$bounds" -G Ninja -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_CXX_FLAGS="-fsanitize=bounds -fsanitize-trap=bounds" > "$w/bounds_cmake.txt" 2>&1; then
  cat "$w/bounds_cmake.txt"; echo "FAIL: bounds build configuration"; exit 1
fi
if ! cmake --build "$bounds" > "$w/bounds_build.txt" 2>&1; then
  grep -E "error" "$w/bounds_build.txt" | head -20; echo "FAIL: bounds build"; exit 1
fi
RSR_BIN="$bounds/rsr" bash "$here/run_scenarios.sh" "$w/bounds" "${opts[@]}"
for ref in "$here"/reference/*.txt; do
  name=$(basename "$ref" .txt)
  if [ ! -f "$w/bounds/$name.ppm" ]; then
    echo "   $name stopped (an index outside a table?): $(tail -1 "$w/bounds/$name.log")"
    echo "FAIL: array bounds"; exit 1
  fi
done
echo "   $(ls "$here"/reference/*.txt | wc -l) scenarios within bounds"

echo "== picture and sound against the reference (tests/reference)"
# Section 10 of docs/history/restructure-strategy.md: since the loader runs the
# modules without processes, --native off (the original loader) lays out
# memory differently, so the RAM comparison with it no longer applies; the
# recordings of the original game's picture and sound do.
ref_fail=0
for ref in "$here"/reference/*.txt; do
  name=$(basename "$ref" .txt)
  if [ "$(grep '^\[avhash\]' "$w/on/$name.log")" != "$(grep '^\[avhash\]' "$ref")" ]; then
    first=$(diff <(grep '^\[avhash\]' "$ref") <(grep '^\[avhash\]' "$w/on/$name.log") | grep -m1 '^[<>]' || true)
    echo "   DIFF $name: first at ${first#[<>] }"; ref_fail=1
  fi
done
[ $ref_fail -eq 0 ] || { echo "FAIL: picture or sound differs from the reference"; exit 1; }
echo "   $(ls "$here"/reference/*.txt | wc -l) scenarios as recorded"

echo "== save and recall (tests/check_save_load.sh)"
if ! bash "$here/check_save_load.sh" "${RSR_DATA:-$root/extracted/RedStormRising/data}" > "$w/save_load.txt" 2>&1; then
  cat "$w/save_load.txt"; echo "FAIL: save and recall"; exit 1
fi
echo "   $(tail -1 "$w/save_load.txt")"

echo "PASS"
