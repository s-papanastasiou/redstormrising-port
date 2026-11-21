#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compare two tests/run_scenarios.sh output directories (run with
# --ram-hash): every scenario must give the same RAM hash stream and the
# same final screenshot. Prints "same NAME (N hashes)" or "DIFF NAME ...".
#   tests/compare_runs.sh DIR_A DIR_B
set -uo pipefail
a=$1 b=$2
status=0
for log in "$a"/*.log; do
  name=$(basename "$log" .log)
  ha=$(grep '^\[ramhash\]' "$log" || true)
  hb=$(grep '^\[ramhash\]' "$b/$name.log" 2>/dev/null || true)
  n=$(printf '%s' "$ha" | grep -c . || true)
  if [ "$n" -eq 0 ]; then
    echo "DIFF $name: no RAM hashes in $log"; status=1; continue
  fi
  if [ "$ha" != "$hb" ]; then
    first=$(diff <(printf '%s\n' "$ha") <(printf '%s\n' "$hb") | grep -m1 '^<' || true)
    echo "DIFF $name: RAM hashes differ, first at: ${first#< }"; status=1; continue
  fi
  if ! cmp -s "$a/$name.ppm" "$b/$name.ppm"; then
    echo "DIFF $name: final screenshots differ"; status=1; continue
  fi
  echo "same $name ($n hashes)"
done
exit $status
