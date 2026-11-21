#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build the Windows package (cross-compiled with MinGW on Linux):
#   tools/package-windows.sh            -> dist/RedStormRising-windows.zip
# Needs mingw64-gcc-c++ and mingw64-SDL3 (Fedora) and zip. The package
# holds the program only; the game's files come from the player's copy
# (packaging/windows/README.txt).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
build="$root/build-win"
sysroot=${MINGW_SYSROOT:-/usr/x86_64-w64-mingw32/sys-root/mingw}
out="$root/dist/RedStormRising-windows"

# The source of exactly this build goes into the package (source.zip, the
# GPL's corresponding source): the commit it is built from.
if [ -n "$(git -C "$root" status --porcelain --untracked-files=no)" ]; then
  echo "package-windows.sh: uncommitted changes: commit them first, so that source.zip matches the program" >&2
  exit 1
fi
if [ ! -f "$build/build.ninja" ]; then
  mingw64-cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release
fi
cmake --build "$build" --target rsr

rm -rf "$out" "$out.zip"
mkdir -p "$out"
cp "$build/rsr.exe" "$out/"
x86_64-w64-mingw32-strip "$out/rsr.exe"  # (the debug information)
cp "$sysroot/bin/SDL3.dll" "$out/"
cp -r "$root/data/shaders" "$out/shaders"
rm -rf "$out/shaders/test"
cp "$root/packaging/windows/run.bat" "$root/packaging/windows/README.txt" "$out/"
cp "$root/docs/keys.pdf" "$out/"
cp "$root/LICENSE" "$out/LICENSE.txt"
git -C "$root" archive --format=zip --prefix=redstormrising-port/ -o "$out/source.zip" HEAD
# Windows line ends for the text files.
sed -i 's/$/\r/' "$out/run.bat" "$out/README.txt" "$out/LICENSE.txt"
(cd "$root/dist" && zip -qr "RedStormRising-windows.zip" "RedStormRising-windows")
echo "$out.zip"
