// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// Optional post-processing presenter: runs a RetroArch/libretro GLSL shader
// preset (.glslp, e.g. crt-royale) over the finished frame and draws the
// result into the current OpenGL framebuffer. Presentation only: the game,
// the emulated machine and the original pixels are unchanged.
//
// Conventions follow RetroArch's GLSL backend: each .glsl file is compiled
// twice with VERTEX / FRAGMENT defined; attributes VertexCoord, TexCoord,
// COLOR; uniforms MVPMatrix, FrameCount, FrameDirection, OutputSize,
// TextureSize, InputSize, Texture, Orig*, PassPrev<N>*, Pass<N>*, <alias>*,
// lookup textures by name, and #pragma parameter values (PARAMETER_UNIFORM
// is defined only for files that declare parameters). Requires OpenGL 3.0+
// (framebuffer objects, sRGB and float render targets, GLSL 1.30).
#pragma once

#include "present/shader_preset.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace present {

using GetProc = void* (*)(const char*);

class ShaderChain {
public:
  ShaderChain();
  ~ShaderChain();
  ShaderChain(const ShaderChain&) = delete;
  ShaderChain& operator=(const ShaderChain&) = delete;

  // Parse the preset and read its shader sources (no GL needed).
  bool load(const std::string& preset_path, std::string& err);
  // Compile/link all passes and load lookup textures. The target window's
  // GL context must be current.
  bool init_gl(GetProc proc, std::string& err);
  // Draw `pixels` (top-down, 0x??RRGGBB, w x h) through the chain into the
  // default framebuffer (drawable_w x drawable_h); the picture fills the
  // rectangle (vp_x, vp_y, vp_w, vp_h), GL bottom-left origin, the rest is
  // cleared to black. The caller swaps buffers.
  bool render(const uint32_t* pixels, int w, int h, int drawable_w, int drawable_h,
              int vp_x, int vp_y, int vp_w, int vp_h);
  // Test aid: read the default framebuffer as top-down RGB (before swap).
  bool read_rgb(std::vector<uint8_t>& rgb, int w, int h);

  const std::string& preset_path() const { return path_; }
  size_t pass_count() const { return preset_.passes.size(); }

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  ShaderPreset preset_;
  std::vector<std::string> sources_;
  std::string path_;
};

// Letterboxed rectangle of aspect `aspect` (width/height) centred in a
// drawable, in GL bottom-left coordinates.
void letterbox(int drawable_w, int drawable_h, double aspect,
               int& x, int& y, int& w, int& h);

}  // namespace present
