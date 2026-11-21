#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Record the reference: every scenario's picture and sound as digests of
# 60-frame blocks (--av-hash 60, --det-timing, as NTSC), into
# tests/reference/<scenario>.txt. Section 10 of
# docs/history/restructure-strategy.md: these recordings are the gate once the
# original code and the RAM image go away (tests/check_reference.sh).
# Record only from a build that passes tests/check_native.sh.
#   tests/record_reference.sh [WORKDIR]
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
w=${1:-$(mktemp -d)}
cmake --build "$root/build" > /dev/null
bash "$here/run_scenarios.sh" "$w" --det-timing --av-hash 60 --reference-terrain
commit=$(git -C "$root" rev-parse --short HEAD)
for log in "$w"/*.log; do
  name=$(basename "$log" .log)
  {
    echo "# $name: --det-timing --av-hash 60 --reference-terrain (NTSC), recorded at $commit"
    grep '^\[avhash\]' "$log"
  } > "$here/reference/$name.txt"
  echo "   $name: $(grep -c '^\[avhash\]' "$log") blocks"
done
