// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// Parser for RetroArch/libretro GLSL shader presets (.glslp), used by the
// optional CRT post-processing presenter (shader_chain.hpp). Independent of
// GL and SDL so it can be unit tested.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace present {

enum class ScaleType { Source, Viewport, Absolute };

struct ShaderPass {
  std::string path;           // resolved .glsl file
  std::string alias;          // optional name other passes can sample
  std::optional<bool> filter_linear;  // how this pass samples its input
  bool mipmap_input = false;
  std::string wrap_mode = "clamp_to_border";
  bool srgb_framebuffer = false;
  bool float_framebuffer = false;
  int frame_count_mod = 0;
  bool scale_given = false;   // any scale_type* key present
  ScaleType scale_type_x = ScaleType::Source, scale_type_y = ScaleType::Source;
  float scale_x = 1.f, scale_y = 1.f;
};

struct ShaderTexture {        // lookup texture (LUT) bound as sampler `name`
  std::string name, path;
  bool linear = false, mipmap = false;
  std::string wrap_mode = "clamp_to_border";
};

struct ShaderPreset {
  std::vector<ShaderPass> passes;
  std::vector<ShaderTexture> textures;
  std::map<std::string, float> parameters;  // preset overrides of #pragma parameter
};

// Parse preset text. Relative paths resolve against `base_dir`. Returns
// false with `err` set on malformed input.
bool parse_preset(const std::string& text, const std::string& base_dir,
                  ShaderPreset& out, std::string& err);
bool load_preset(const std::string& path, ShaderPreset& out, std::string& err);

// Output size of a pass given its input size and the final viewport size.
// The last pass of a chain always renders to the viewport unless it
// declares its own scale.
void pass_output_size(const ShaderPass& pass, bool last, int in_w, int in_h,
                      int vp_w, int vp_h, int& out_w, int& out_h);

}  // namespace present
