#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Adapted from fa18-interceptor-port tests/check_shader_present.sh.
# The optional shader presenter (--shader-preset) must draw the emulated
# frame through a libretro GLSL preset into the window. Uses the two-pass
# test preset in data/shaders/test: at the centre of every source row its
# output equals the game pixel (orientation, scaling and the sRGB/alias/Orig
# bindings all have to be right), and between rows it is darker. A broken
# preset must fall back to the normal SDL presentation.
set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "usage: $0 DATA_DIR [rsr-executable]" >&2
  exit 2
fi
repo=$(cd "$(dirname "$0")/.." && pwd)
adf=$1
bin=${2:-"$repo/build/rsr"}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
# Frame 2500: the finished title logo (static).
save=(--save "$tmp/save")

RSR_SHADER_DEBUG=1 RSR_PRESENT_SHOT="$tmp/window.ppm" "$bin" --ntsc "${save[@]}" --fast --frames 2505 \
  --screenshot 2500 "$tmp/game.ppm" \
  --shader-preset "$repo/data/shaders/test/scanline-test.glslp" "$adf" >"$tmp/run.log" 2>&1
grep -q 'shader: .*scanline-test.glslp (2 passes)' "$tmp/run.log"
if grep -q 'shader: GL error\|using normal output' "$tmp/run.log"; then
  cat "$tmp/run.log" >&2
  echo 'FAIL: shader presenter reported an error' >&2
  exit 1
fi

python3 -I - "$tmp/game.ppm" "$tmp/window.ppm" <<'PY'
import sys
def ppm(path):
    magic, dims, maxval, pixels = open(path, 'rb').read().split(b'\n', 3)
    w, h = map(int, dims.split())
    return w, h, pixels
gw, gh, game = ppm(sys.argv[1])
ww, wh, win = ppm(sys.argv[2])
scale = min(ww / gw, wh / (gh * 6 / 5))
vw, vh = gw * scale, gh * 6 / 5 * scale
ox, oy = (ww - vw) / 2, (wh - vh) / 2
def px(buf, w, x, y):
    i = (y * w + x) * 3
    return buf[i:i + 3]
centre_ok = total = 0
centre_sum = edge_sum = 0
for y in range(2, gh - 2):
    for x in range(0, gw, 4):
        sx = int(ox + (x + 0.5) * vw / gw)
        g = px(game, gw, x, y)
        c = px(win, ww, sx, int(oy + (y + 0.5) * vh / gh))
        e = px(win, ww, sx, min(wh - 1, int(oy + (y + 1.0) * vh / gh)))
        total += 1
        centre_ok += all(abs(a - b) <= 6 for a, b in zip(g, c))
        centre_sum += sum(c)
        edge_sum += sum(e)
print(f"window {ww}x{wh}: {centre_ok}/{total} row centres match the game, "
      f"row edges at {edge_sum / max(1, centre_sum):.2f} of centre brightness")
if centre_ok < total * 97 // 100:
    sys.exit("FAIL: shader output does not show the emulated frame at row centres")
if edge_sum > centre_sum * 0.85:
    sys.exit("FAIL: no scanline darkening between source rows")
PY

# A preset that does not compile must not break presentation.
mkdir -p "$tmp/bad"
printf 'shaders = "1"\nshader0 = "bad.glsl"\n' >"$tmp/bad/bad.glslp"
printf '#version 130\nthis is not glsl\n' >"$tmp/bad/bad.glsl"
RSR_PRESENT_SHOT="$tmp/fallback.ppm" "$bin" --ntsc "${save[@]}" --fast --frames 2505 --screenshot 2500 "$tmp/game2.ppm" \
  --shader-preset "$tmp/bad/bad.glslp" "$adf" >"$tmp/bad.log" 2>&1
grep -q 'using normal output' "$tmp/bad.log"
cmp -s <(tail -c +16 "$tmp/fallback.ppm" | head -c 1000) /dev/null && {
  echo 'FAIL: no fallback presentation' >&2; exit 1; }
test -s "$tmp/fallback.ppm"
# The bundled crt-royale preset: all 12 passes, no GL errors, and a lit
# picture that tracks the game (brightness correlates with the frame).
RSR_SHADER_DEBUG=1 RSR_PRESENT_SHOT="$tmp/royale.ppm" "$bin" --ntsc "${save[@]}" --fast --frames 2505 \
  --screenshot 2500 "$tmp/game3.ppm" --crt-royale "$adf" >"$tmp/royale.log" 2>&1
grep -q 'shader: .*libretro/crt/crt-royale.glslp (12 passes)' "$tmp/royale.log"
if grep -q 'shader: GL error\|using normal output' "$tmp/royale.log"; then
  cat "$tmp/royale.log" >&2
  echo 'FAIL: crt-royale reported an error' >&2
  exit 1
fi
python3 -I - "$tmp/game3.ppm" "$tmp/royale.ppm" <<'PY'
import sys
def ppm(path):
    magic, dims, maxval, pixels = open(path, 'rb').read().split(b'\n', 3)
    w, h = map(int, dims.split())
    return w, h, pixels
gw, gh, game = ppm(sys.argv[1])
ww, wh, win = ppm(sys.argv[2])
# Compare 8x8-cell average luminance (the CRT mask/bloom alter single pixels).
cells = []
for cy in range(2, 23):
    for cx in range(2, 38):
        def lum(buf, w, x0, y0, x1, y1):
            t = n = 0
            for y in range(y0, y1):
                for x in range(x0, x1):
                    i = (y * w + x) * 3
                    t += buf[i] + buf[i + 1] + buf[i + 2]; n += 1
            return t / n
        g = lum(game, gw, cx * 8, cy * 8, cx * 8 + 8, cy * 8 + 8)
        sx0, sx1 = cx * 8 * ww // gw, (cx * 8 + 8) * ww // gw
        sy0, sy1 = cy * 8 * wh // gh, (cy * 8 + 8) * wh // gh
        cells.append((g, lum(win, ww, sx0, sy0, sx1, sy1)))
n = len(cells)
mg = sum(g for g, _ in cells) / n; mw = sum(w for _, w in cells) / n
cov = sum((g - mg) * (w - mw) for g, w in cells)
vg = sum((g - mg) ** 2 for g, _ in cells); vw = sum((w - mw) ** 2 for _, w in cells)
r = cov / (vg * vw) ** 0.5 if vg and vw else 0
print(f"crt-royale {ww}x{wh}: mean brightness {mw / 3:.0f}, correlation with game {r:.2f}")
if mw / 3 < 20 or r < 0.8:
    sys.exit("FAIL: crt-royale output does not show the emulated frame")
PY
# The bundled crt-lottes-fast preset: its single pass, no GL errors, and a lit
# picture that tracks the game (brightness correlates with the frame).
RSR_SHADER_DEBUG=1 RSR_PRESENT_SHOT="$tmp/lottesfast.ppm" "$bin" --ntsc "${save[@]}" --fast --frames 2505 \
  --screenshot 2500 "$tmp/game5.ppm" --crt-lottes-fast "$adf" >"$tmp/lottesfast.log" 2>&1
grep -q 'shader: .*libretro/crt/crt-lottes-fast.glslp (1 passes)' "$tmp/lottesfast.log"
if grep -q 'shader: GL error\|using normal output' "$tmp/lottesfast.log"; then
  cat "$tmp/lottesfast.log" >&2
  echo 'FAIL: crt-lottes-fast reported an error' >&2
  exit 1
fi
python3 -I - "$tmp/game5.ppm" "$tmp/lottesfast.ppm" <<'PY'
import sys
def ppm(path):
    magic, dims, maxval, pixels = open(path, 'rb').read().split(b'\n', 3)
    w, h = map(int, dims.split())
    return w, h, pixels
gw, gh, game = ppm(sys.argv[1])
ww, wh, win = ppm(sys.argv[2])
# Compare 8x8-cell average luminance (the CRT mask/bloom alter single pixels).
cells = []
for cy in range(2, 23):
    for cx in range(2, 38):
        def lum(buf, w, x0, y0, x1, y1):
            t = n = 0
            for y in range(y0, y1):
                for x in range(x0, x1):
                    i = (y * w + x) * 3
                    t += buf[i] + buf[i + 1] + buf[i + 2]; n += 1
            return t / n
        g = lum(game, gw, cx * 8, cy * 8, cx * 8 + 8, cy * 8 + 8)
        sx0, sx1 = cx * 8 * ww // gw, (cx * 8 + 8) * ww // gw
        sy0, sy1 = cy * 8 * wh // gh, (cy * 8 + 8) * wh // gh
        cells.append((g, lum(win, ww, sx0, sy0, sx1, sy1)))
n = len(cells)
mg = sum(g for g, _ in cells) / n; mw = sum(w for _, w in cells) / n
cov = sum((g - mg) * (w - mw) for g, w in cells)
vg = sum((g - mg) ** 2 for g, _ in cells); vw = sum((w - mw) ** 2 for _, w in cells)
r = cov / (vg * vw) ** 0.5 if vg and vw else 0
print(f"crt-lottes-fast {ww}x{wh}: mean brightness {mw / 3:.0f}, correlation with game {r:.2f}")
if mw / 3 < 20 or r < 0.8:
    sys.exit("FAIL: crt-lottes-fast output does not show the emulated frame")
PY
# The bundled crt-guest preset: all 11 passes, no GL errors, and a lit
# picture that tracks the game (brightness correlates with the frame).
RSR_SHADER_DEBUG=1 RSR_PRESENT_SHOT="$tmp/guest.ppm" "$bin" --ntsc "${save[@]}" --fast --frames 2505 \
  --screenshot 2500 "$tmp/game6.ppm" --crt-guest "$adf" >"$tmp/guest.log" 2>&1
grep -q 'shader: .*libretro/crt/crt-guest-dr-venom.glslp (11 passes)' "$tmp/guest.log"
if grep -q 'shader: GL error\|using normal output' "$tmp/guest.log"; then
  cat "$tmp/guest.log" >&2
  echo 'FAIL: crt-guest reported an error' >&2
  exit 1
fi
python3 -I - "$tmp/game6.ppm" "$tmp/guest.ppm" <<'PY'
import sys
def ppm(path):
    magic, dims, maxval, pixels = open(path, 'rb').read().split(b'\n', 3)
    w, h = map(int, dims.split())
    return w, h, pixels
gw, gh, game = ppm(sys.argv[1])
ww, wh, win = ppm(sys.argv[2])
# Compare 8x8-cell average luminance (the CRT mask/bloom alter single pixels).
cells = []
for cy in range(2, 23):
    for cx in range(2, 38):
        def lum(buf, w, x0, y0, x1, y1):
            t = n = 0
            for y in range(y0, y1):
                for x in range(x0, x1):
                    i = (y * w + x) * 3
                    t += buf[i] + buf[i + 1] + buf[i + 2]; n += 1
            return t / n
        g = lum(game, gw, cx * 8, cy * 8, cx * 8 + 8, cy * 8 + 8)
        sx0, sx1 = cx * 8 * ww // gw, (cx * 8 + 8) * ww // gw
        sy0, sy1 = cy * 8 * wh // gh, (cy * 8 + 8) * wh // gh
        cells.append((g, lum(win, ww, sx0, sy0, sx1, sy1)))
n = len(cells)
mg = sum(g for g, _ in cells) / n; mw = sum(w for _, w in cells) / n
cov = sum((g - mg) * (w - mw) for g, w in cells)
vg = sum((g - mg) ** 2 for g, _ in cells); vw = sum((w - mw) ** 2 for _, w in cells)
r = cov / (vg * vw) ** 0.5 if vg and vw else 0
print(f"crt-guest {ww}x{wh}: mean brightness {mw / 3:.0f}, correlation with game {r:.2f}")
if mw / 3 < 20 or r < 0.8:
    sys.exit("FAIL: crt-guest output does not show the emulated frame")
PY
# The bundled crt-guest-fast preset: all 5 passes, no GL errors, and a lit
# picture that tracks the game (brightness correlates with the frame).
RSR_SHADER_DEBUG=1 RSR_PRESENT_SHOT="$tmp/guestfast.ppm" "$bin" --ntsc "${save[@]}" --fast --frames 2505 \
  --screenshot 2500 "$tmp/game7.ppm" --crt-guest-fast "$adf" >"$tmp/guestfast.log" 2>&1
grep -q 'shader: .*libretro/crt/crt-guest-dr-venom-fast.glslp (5 passes)' "$tmp/guestfast.log"
if grep -q 'shader: GL error\|using normal output' "$tmp/guestfast.log"; then
  cat "$tmp/guestfast.log" >&2
  echo 'FAIL: crt-guest-fast reported an error' >&2
  exit 1
fi
python3 -I - "$tmp/game7.ppm" "$tmp/guestfast.ppm" <<'PY'
import sys
def ppm(path):
    magic, dims, maxval, pixels = open(path, 'rb').read().split(b'\n', 3)
    w, h = map(int, dims.split())
    return w, h, pixels
gw, gh, game = ppm(sys.argv[1])
ww, wh, win = ppm(sys.argv[2])
# Compare 8x8-cell average luminance (the CRT mask/bloom alter single pixels).
cells = []
for cy in range(2, 23):
    for cx in range(2, 38):
        def lum(buf, w, x0, y0, x1, y1):
            t = n = 0
            for y in range(y0, y1):
                for x in range(x0, x1):
                    i = (y * w + x) * 3
                    t += buf[i] + buf[i + 1] + buf[i + 2]; n += 1
            return t / n
        g = lum(game, gw, cx * 8, cy * 8, cx * 8 + 8, cy * 8 + 8)
        sx0, sx1 = cx * 8 * ww // gw, (cx * 8 + 8) * ww // gw
        sy0, sy1 = cy * 8 * wh // gh, (cy * 8 + 8) * wh // gh
        cells.append((g, lum(win, ww, sx0, sy0, sx1, sy1)))
n = len(cells)
mg = sum(g for g, _ in cells) / n; mw = sum(w for _, w in cells) / n
cov = sum((g - mg) * (w - mw) for g, w in cells)
vg = sum((g - mg) ** 2 for g, _ in cells); vw = sum((w - mw) ** 2 for _, w in cells)
r = cov / (vg * vw) ** 0.5 if vg and vw else 0
print(f"crt-guest-fast {ww}x{wh}: mean brightness {mw / 3:.0f}, correlation with game {r:.2f}")
if mw / 3 < 20 or r < 0.8:
    sys.exit("FAIL: crt-guest-fast output does not show the emulated frame")
PY
echo 'PASS: shader preset presents the frame; crt-royale, crt-lottes-fast, crt-guest and crt-guest-fast run; broken presets fall back to SDL'
