// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

#include "present/shader_preset.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <string>

using namespace present;

int main() {
  // Excerpt in the style of crt-royale.glslp: comments (also after values),
  // per-axis scale types, aliases, LUTs and relative paths.
  const std::string text = R"(
# comment
shaders = "3"
textures = "mask_small;mask_large"
mask_small = "masks/small.png"
mask_large = "masks/large.png"
mask_small_linear = "true"
mask_large_mipmap = "true"  # trailing comment
mask_large_wrap_mode = "repeat"

shader0 = "src/first.glsl"
alias0 = "ORIG_LINEARIZED"
filter_linear0 = "false"
scale_type0 = "source"
scale0 = "1.0"
srgb_framebuffer0 = "true"

shader1 = "../blurs/blur.glsl"
scale_type_x1 = "absolute"
scale_x1 = "64"
scale_type_y1 = "viewport"
scale_y1 = "0.0625"
float_framebuffer1 = "true"
mipmap_input1 = "true"
texture_wrap_mode1 = "clamp_to_edge"

shader2 = "src/last.glsl"
filter_linear2 = "true"
frame_count_mod2 = "2"

parameters = "crt_gamma"
crt_gamma = "2.4"
)";
  ShaderPreset p;
  std::string err;
  assert(parse_preset(text, "/presets/crt", p, err));
  assert(p.passes.size() == 3);
  assert(p.passes[0].path == "/presets/crt/src/first.glsl");
  assert(p.passes[0].alias == "ORIG_LINEARIZED");
  assert(p.passes[0].filter_linear && !*p.passes[0].filter_linear);
  assert(p.passes[0].srgb_framebuffer && !p.passes[0].float_framebuffer);
  assert(p.passes[1].path == "/presets/blurs/blur.glsl");
  assert(p.passes[1].float_framebuffer && p.passes[1].mipmap_input);
  assert(p.passes[1].wrap_mode == "clamp_to_edge");
  assert(p.passes[1].scale_type_x == ScaleType::Absolute);
  assert(p.passes[1].scale_type_y == ScaleType::Viewport);
  assert(!p.passes[2].scale_given && p.passes[2].frame_count_mod == 2);
  assert(p.textures.size() == 2);
  assert(p.textures[0].name == "mask_small" && p.textures[0].linear && !p.textures[0].mipmap);
  assert(p.textures[1].path == "/presets/crt/masks/large.png");
  assert(p.textures[1].mipmap && p.textures[1].wrap_mode == "repeat");
  assert(p.parameters.at("crt_gamma") == 2.4f);

  // Output sizes: source/absolute/viewport scales; an unscaled last pass
  // fills the viewport.
  int w = 0, h = 0;
  pass_output_size(p.passes[0], false, 320, 200, 960, 720, w, h);
  assert(w == 320 && h == 200);
  pass_output_size(p.passes[1], false, 320, 200, 960, 720, w, h);
  assert(w == 64 && h == 45);
  pass_output_size(p.passes[2], true, 64, 45, 960, 720, w, h);
  assert(w == 960 && h == 720);

  // Errors are reported, not ignored.
  assert(!parse_preset("shaders = \"1\"\n", "", p, err) && !err.empty());
  assert(!parse_preset("garbage\n", "", p, err));
  assert(!parse_preset("shaders = 1\nshader0 = a.glsl\nscale_type0 = bogus\n", "", p, err));
}
