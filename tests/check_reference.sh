#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compare every scenario's picture and sound with the recorded reference
# (tests/record_reference.sh). On a difference it names the first 60-frame
# block that differs; rerun both builds with --av-hash 1 to find the frame.
#   tests/check_reference.sh [WORKDIR [rsr options...]]
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
w=${1:-$(mktemp -d)}
shift || true
if ! cmake --build "$root/build" > /dev/null; then echo "FAIL: build"; exit 1; fi
bash "$here/run_scenarios.sh" "$w" --det-timing --av-hash 60 --reference-terrain "$@"
status=0
for ref in "$here"/reference/*.txt; do
  name=$(basename "$ref" .txt)
  got=$(grep '^\[avhash\]' "$w/$name.log" 2>/dev/null || true)
  want=$(grep '^\[avhash\]' "$ref")
  if [ "$got" == "$want" ]; then
    echo "same $name ($(printf '%s\n' "$want" | wc -l) blocks)"
  else
    first=$(diff <(printf '%s\n' "$want") <(printf '%s\n' "$got") | grep -m1 '^[<>]' || true)
    echo "DIFF $name: first at ${first#[<>] }"
    status=1
  fi
done
[ $status -eq 0 ] && echo PASS || echo "FAIL: picture or sound differs from the reference"
exit $status
