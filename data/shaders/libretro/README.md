# Bundled libretro shaders

Files copied unmodified from <https://github.com/libretro/glsl-shaders>
(commit `f277bd87e454a68c50e01e61e3588118d56b1365`, 2026-11-07), keeping the
upstream directory layout so the preset's relative paths resolve. Only the
files used by `crt/crt-royale.glslp`, `crt/crt-lottes-fast.glslp`,
`crt/crt-guest-dr-venom.glslp` and `crt/crt-guest-dr-venom-fast.glslp` are
included. Used by `rsr
--crt-royale` / `--crt-lottes-fast` / `--crt-guest` / `--crt-guest-fast` (equivalent to `--shader-preset` on those
presets).

| Files | Author | License |
|---|---|---|
| `crt/crt-royale.glslp`, `crt/shaders/crt-royale/**` | TroggleMonkey (crt-royale) | GPL v2 or later (`crt/shaders/crt-royale/LICENSE.TXT`) |
| `blurs/shaders/royale/blur9fast-*.glsl` | TroggleMonkey | MIT (notice in each file) |
| `crt/crt-lottes-fast.glslp`, `crt/shaders/crt-lottes-fast.glsl` | Timothy Lottes | Public domain / Unlicense (notice in the file) |
| `crt/crt-guest-dr-venom*.glslp`, `crt/shaders/guest/**` | guest(r), with ideas from Dr. Venom | GPL v2 or later (notices in the main passes; the small helper passes without a notice and the `lut/*.png` colour tables, credited to torridgristle in `lut/README`, are distributed by libretro as part of the same package) |

See `crt/shaders/crt-royale/README.TXT` for the shader's documentation and
settings (compile-time defaults in the pass sources).
