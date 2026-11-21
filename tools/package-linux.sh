#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build the Linux package:
#   tools/package-linux.sh              -> dist/RedStormRising-linux-x86_64.tar.gz
# Needs CMake, Ninja, a C++20 compiler and SDL3 (its development files).
# The package carries its own copy of SDL3 (lib/), and the C++ runtime is
# linked in, so it runs on distributions as old as the one it is built on
# (the release builds use Ubuntu 22.04: .github/workflows). The package
# holds the program only; the game's files come from the player's copy
# (packaging/linux/README.txt). CMAKE_PREFIX_PATH finds an SDL3 built
# elsewhere.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
build="$root/build-linux-package"
out="$root/dist/RedStormRising-linux-x86_64"

# The source of exactly this build goes into the package (source.zip, the
# GPL's corresponding source): the commit it is built from.
if [ -n "$(git -C "$root" status --porcelain --untracked-files=no)" ]; then
  echo "package-linux.sh: uncommitted changes: commit them first, so that source.zip matches the program" >&2
  exit 1
fi
cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRSR_PORTABLE=ON
cmake --build "$build" --target rsr

rm -rf "$out" "$out.tar.gz"
mkdir -p "$out/lib"
cp "$build/rsr" "$out/"
strip "$out/rsr"  # (the debug information)
# SDL3, as the program found it when linked (the .so.0 it loads).
sdl=$(ldd "$build/rsr" | awk '/libSDL3\.so/ {print $3; exit}')
if [ -z "$sdl" ] || [ ! -f "$sdl" ]; then
  echo "package-linux.sh: libSDL3.so not found by ldd" >&2
  exit 1
fi
cp -L "$sdl" "$out/lib/$(basename "$sdl")"
strip --strip-unneeded "$out/lib/$(basename "$sdl")"
cp -r "$root/data/shaders" "$out/shaders"
rm -rf "$out/shaders/test"
cp "$root/packaging/linux/README.txt" "$out/"
cp "$root/docs/keys.pdf" "$out/"
cp "$root/LICENSE" "$out/LICENSE.txt"
git -C "$root" archive --format=zip --prefix=redstormrising-port/ -o "$out/source.zip" HEAD
tar -C "$root/dist" -czf "$out.tar.gz" "$(basename "$out")"
echo "$out.tar.gz"
