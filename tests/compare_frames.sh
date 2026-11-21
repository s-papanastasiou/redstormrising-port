#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compare the rendered frames of two tests/run_scenarios.sh output
# directories (run with --ram-hash N): the [fbhash] streams must be equal.
# For changes to the display emulation, which the native gate cannot see
# (native off and on render with the same code): run the scenarios with
# the old and the new build and compare.
#   tests/compare_frames.sh DIR_A DIR_B
set -uo pipefail
a=$1 b=$2
status=0
for log in "$a"/*.log; do
  name=$(basename "$log" .log)
  ha=$(grep '^\[fbhash\]' "$log" || true)
  hb=$(grep '^\[fbhash\]' "$b/$name.log" 2>/dev/null || true)
  n=$(printf '%s' "$ha" | grep -c . || true)
  if [ "$n" -eq 0 ]; then echo "DIFF $name: no frame hashes in $log"; status=1; continue; fi
  if [ "$ha" != "$hb" ]; then
    first=$(diff <(printf '%s\n' "$ha") <(printf '%s\n' "$hb") | grep -m1 '^<' || true)
    echo "DIFF $name: frames differ, first at: ${first#< }"; status=1; continue
  fi
  echo "same $name ($n frames)"
done
exit $status
