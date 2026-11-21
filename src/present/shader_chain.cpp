// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// Copied from fa18-interceptor-port src/present (updated for frame history).
#include "present/shader_chain.hpp"

#define GL_GLEXT_PROTOTYPES 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>

namespace present {

namespace {

// Entry points, resolved through the host's GetProcAddress. The prototypes
// from SDL_opengl.h are only used for their types (no link-time GL needed).
#define RSR_GL_FUNCS(X)                                                        \
  X(glGetString) X(glGetError) X(glViewport) X(glClearColor) X(glClear)         \
  X(glEnable) X(glDisable) X(glPixelStorei) X(glReadPixels)                     \
  X(glGenTextures) X(glDeleteTextures) X(glBindTexture) X(glTexImage2D)        \
  X(glTexSubImage2D) X(glTexParameteri) X(glTexParameterfv) X(glActiveTexture)  \
  X(glGenerateMipmap) X(glGenFramebuffers) X(glDeleteFramebuffers)             \
  X(glBindFramebuffer) X(glFramebufferTexture2D) X(glCheckFramebufferStatus)   \
  X(glCreateShader) X(glShaderSource) X(glCompileShader) X(glGetShaderiv)      \
  X(glGetShaderInfoLog) X(glDeleteShader) X(glCreateProgram) X(glAttachShader) \
  X(glBindAttribLocation) X(glLinkProgram) X(glGetProgramiv)                   \
  X(glGetProgramInfoLog) X(glDeleteProgram) X(glUseProgram)                    \
  X(glGetUniformLocation) X(glUniform1i) X(glUniform1f) X(glUniform2f)          \
  X(glUniformMatrix4fv) X(glGenBuffers) X(glDeleteBuffers) X(glBindBuffer)      \
  X(glBufferData) X(glVertexAttribPointer) X(glEnableVertexAttribArray)        \
  X(glDrawArrays)

struct Gl {
#define RSR_GL_DECL(name) decltype(&::name) name = nullptr;
  RSR_GL_FUNCS(RSR_GL_DECL)
#undef RSR_GL_DECL
  // Optional (core profiles need a bound vertex array object).
  PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = nullptr;
  PFNGLBINDVERTEXARRAYPROC glBindVertexArray = nullptr;
  PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = nullptr;
};

struct Target {         // a pass output (or the uploaded source frame)
  GLuint tex = 0, fbo = 0;
  int w = 0, h = 0;
  GLenum format = 0;
};

struct Program {
  GLuint prog = 0;
  std::map<std::string, GLint> loc;
  std::vector<std::pair<std::string, float>> params;  // #pragma parameter values
  GLint uniform(const Gl& gl, const std::string& name) {
    auto it = loc.find(name);
    if (it != loc.end()) return it->second;
    const GLint l = gl.glGetUniformLocation(prog, name.c_str());
    loc[name] = l;
    return l;
  }
};

struct Lut {
  std::string name;
  GLuint tex = 0;
  int w = 0, h = 0;
};

GLenum wrap_enum(const std::string& mode) {
  if (mode == "repeat") return GL_REPEAT;
  if (mode == "mirrored_repeat") return GL_MIRRORED_REPEAT;
  if (mode == "clamp_to_edge") return GL_CLAMP_TO_EDGE;
  return GL_CLAMP_TO_BORDER;
}

// #pragma parameter NAME "description" default min max [step]
std::vector<std::pair<std::string, float>> parse_parameters(const std::string& src) {
  std::vector<std::pair<std::string, float>> out;
  std::istringstream in(src);
  std::string line;
  while (std::getline(in, line)) {
    const size_t p = line.find("#pragma parameter");
    if (p == std::string::npos) continue;
    std::istringstream ls(line.substr(p + 17));
    std::string name;
    ls >> name;
    const size_t q1 = line.find('"'), q2 = q1 == std::string::npos ? q1 : line.find('"', q1 + 1);
    if (name.empty() || q2 == std::string::npos) continue;
    std::istringstream rest(line.substr(q2 + 1));
    float def = 0.f;
    if (rest >> def) out.push_back({name, def});
  }
  return out;
}

// Insert `defines` after the #version line (which must stay first).
std::string with_defines(const std::string& src, const std::string& defines) {
  size_t start = src.find_first_not_of(" \t\r\n");
  if (start != std::string::npos && src.compare(start, 8, "#version") == 0) {
    const size_t eol = src.find('\n', start);
    if (eol == std::string::npos) return src + "\n" + defines;
    return src.substr(0, eol + 1) + defines + src.substr(eol + 1);
  }
  return "#version 130\n" + defines + src;
}

}  // namespace

struct ShaderChain::Impl {
  Gl gl;
  bool ready = false;
  std::vector<Program> programs;
  std::vector<Target> outputs;   // one per pass except the last
  // Uploaded frames. With frame history (a pass samples PrevTexture /
  // Prev1..Prev6Texture, e.g. crt-guest-dr-venom's afterglow) this is a
  // ring of 8 and each frame overwrites the oldest; `source` is the newest.
  std::vector<Target> frames;
  size_t head = 0;
  Target source;
  std::vector<Lut> luts;
  GLuint vbo = 0, vao = 0;
  uint64_t frame_count = 0;
  std::vector<uint8_t> upload;

  ~Impl() {
    if (!ready) return;
    for (auto& p : programs) if (p.prog) gl.glDeleteProgram(p.prog);
    for (auto& t : outputs) {
      if (t.fbo) gl.glDeleteFramebuffers(1, &t.fbo);
      if (t.tex) gl.glDeleteTextures(1, &t.tex);
    }
    for (auto& f : frames)
      if (f.tex) gl.glDeleteTextures(1, &f.tex);
    for (auto& l : luts) if (l.tex) gl.glDeleteTextures(1, &l.tex);
    if (vbo) gl.glDeleteBuffers(1, &vbo);
    if (vao && gl.glDeleteVertexArrays) gl.glDeleteVertexArrays(1, &vao);
  }

  // Texture parameters for a texture sampled as the input of `pass`.
  void set_sampling(GLuint tex, const ShaderPass& pass, bool has_mipmaps) {
    const bool linear = pass.filter_linear.value_or(false);
    gl.glBindTexture(GL_TEXTURE_2D, tex);
    GLint min = linear ? GL_LINEAR : GL_NEAREST;
    if (has_mipmaps) min = linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    const GLenum wrap = wrap_enum(pass.wrap_mode);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLint(wrap));
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLint(wrap));
    const float black[4] = {0, 0, 0, 0};
    gl.glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, black);
  }
};

ShaderChain::ShaderChain() = default;
ShaderChain::~ShaderChain() = default;

bool ShaderChain::load(const std::string& preset_path, std::string& err) {
  path_ = preset_path;
  if (!load_preset(preset_path, preset_, err)) return false;
  sources_.clear();
  for (const auto& pass : preset_.passes) {
    std::ifstream in(pass.path);
    if (!in) { err = "cannot open shader " + pass.path; return false; }
    std::stringstream ss;
    ss << in.rdbuf();
    sources_.push_back(ss.str());
  }
  return true;
}

bool ShaderChain::init_gl(GetProc proc, std::string& err) {
  impl_ = std::make_unique<Impl>();
  Gl& gl = impl_->gl;
  bool missing = false;
#define RSR_GL_LOAD(name)                                                     \
  gl.name = reinterpret_cast<decltype(gl.name)>(proc(#name));                  \
  if (!gl.name) { err = "missing GL entry point " #name; missing = true; }
  RSR_GL_FUNCS(RSR_GL_LOAD)
#undef RSR_GL_LOAD
  if (missing) return false;
  gl.glGenVertexArrays = reinterpret_cast<PFNGLGENVERTEXARRAYSPROC>(proc("glGenVertexArrays"));
  gl.glBindVertexArray = reinterpret_cast<PFNGLBINDVERTEXARRAYPROC>(proc("glBindVertexArray"));
  gl.glDeleteVertexArrays =
      reinterpret_cast<PFNGLDELETEVERTEXARRAYSPROC>(proc("glDeleteVertexArrays"));

  const char* version = reinterpret_cast<const char*>(gl.glGetString(GL_VERSION));
  if (!version || std::strstr(version, "OpenGL ES") || std::atoi(version) < 3) {
    err = std::string("needs desktop OpenGL 3.0 or newer (have ") +
          (version ? version : "unknown") + ")";
    return false;
  }

  // Compile every pass.
  for (size_t i = 0; i < preset_.passes.size(); ++i) {
    const std::string& src = sources_[i];
    Program p;
    p.params = parse_parameters(src);
    for (auto& [name, value] : p.params) {
      auto it = preset_.parameters.find(name);
      if (it != preset_.parameters.end()) value = it->second;
    }
    const std::string common = p.params.empty() ? "" : "#define PARAMETER_UNIFORM\n";
    auto compile = [&](GLenum type, const char* stage) -> GLuint {
      const std::string text = with_defines(src, std::string("#define ") + stage + "\n" + common);
      const GLuint s = gl.glCreateShader(type);
      const char* ptr = text.c_str();
      gl.glShaderSource(s, 1, &ptr, nullptr);
      gl.glCompileShader(s);
      GLint ok = 0;
      gl.glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
      if (!ok) {
        char log[4096] = {};
        gl.glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        err = preset_.passes[i].path + " (" + stage + "): " + log;
        gl.glDeleteShader(s);
        return 0;
      }
      return s;
    };
    const GLuint vs = compile(GL_VERTEX_SHADER, "VERTEX");
    if (!vs) return false;
    const GLuint fs = compile(GL_FRAGMENT_SHADER, "FRAGMENT");
    if (!fs) { gl.glDeleteShader(vs); return false; }
    p.prog = gl.glCreateProgram();
    gl.glAttachShader(p.prog, vs);
    gl.glAttachShader(p.prog, fs);
    gl.glBindAttribLocation(p.prog, 0, "VertexCoord");
    gl.glBindAttribLocation(p.prog, 1, "TexCoord");
    gl.glBindAttribLocation(p.prog, 2, "COLOR");
    gl.glLinkProgram(p.prog);
    gl.glDeleteShader(vs);
    gl.glDeleteShader(fs);
    GLint linked = 0;
    gl.glGetProgramiv(p.prog, GL_LINK_STATUS, &linked);
    if (!linked) {
      char log[4096] = {};
      gl.glGetProgramInfoLog(p.prog, sizeof(log), nullptr, log);
      err = preset_.passes[i].path + " (link): " + log;
      gl.glDeleteProgram(p.prog);
      return false;
    }
    impl_->programs.push_back(std::move(p));
  }

  // Lookup textures (PNG, top row first like the frame).
  for (const auto& t : preset_.textures) {
    SDL_Surface* s = SDL_LoadPNG(t.path.c_str());
    SDL_Surface* rgba = s ? SDL_ConvertSurface(s, SDL_PIXELFORMAT_ABGR8888) : nullptr;
    SDL_DestroySurface(s);
    if (!rgba) {
      err = "cannot load texture " + t.path + ": " + SDL_GetError();
      return false;
    }
    Lut lut;
    lut.name = t.name;
    lut.w = rgba->w;
    lut.h = rgba->h;
    gl.glGenTextures(1, &lut.tex);
    gl.glBindTexture(GL_TEXTURE_2D, lut.tex);
    gl.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gl.glPixelStorei(GL_UNPACK_ROW_LENGTH, rgba->pitch / 4);
    gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgba->w, rgba->h, 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, rgba->pixels);
    gl.glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    SDL_DestroySurface(rgba);
    if (t.mipmap) gl.glGenerateMipmap(GL_TEXTURE_2D);
    GLint min = t.linear ? GL_LINEAR : GL_NEAREST;
    if (t.mipmap) min = t.linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, t.linear ? GL_LINEAR : GL_NEAREST);
    const GLenum wrap = wrap_enum(t.wrap_mode);
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLint(wrap));
    gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLint(wrap));
    impl_->luts.push_back(lut);
  }

  // Full-screen quad: VertexCoord (x, y, 0, 1), TexCoord for FBO passes
  // (t = y: texture row 0 is the image top) and for the final pass to the
  // window (t = 1 - y), COLOR white.
  const float quad[] = {
      // VertexCoord        TexCoord (fbo)   TexCoord (window)  COLOR
      0, 0, 0, 1,           0, 0, 0, 0,      0, 1, 0, 0,        1, 1, 1, 1,
      1, 0, 0, 1,           1, 0, 0, 0,      1, 1, 0, 0,        1, 1, 1, 1,
      0, 1, 0, 1,           0, 1, 0, 0,      0, 0, 0, 0,        1, 1, 1, 1,
      1, 1, 0, 1,           1, 1, 0, 0,      1, 0, 0, 0,        1, 1, 1, 1,
  };
  if (gl.glGenVertexArrays && gl.glBindVertexArray) {
    gl.glGenVertexArrays(1, &impl_->vao);
    gl.glBindVertexArray(impl_->vao);
  }
  gl.glGenBuffers(1, &impl_->vbo);
  gl.glBindBuffer(GL_ARRAY_BUFFER, impl_->vbo);
  gl.glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
  // Frame history only when a pass asks for it.
  // (\bPrev: not PassPrev<N>Texture, which names an earlier pass.)
  static const std::regex prev_re(R"(\bPrev[1-6]?Texture\b)");
  bool history = false;
  for (const auto& src : sources_) history |= std::regex_search(src, prev_re);
  impl_->frames.resize(history ? 8 : 1);
  for (auto& f : impl_->frames) gl.glGenTextures(1, &f.tex);
  impl_->ready = true;
  return true;
}

bool ShaderChain::render(const uint32_t* pixels, int w, int h, int drawable_w,
                         int drawable_h, int vp_x, int vp_y, int vp_w, int vp_h) {
  if (!impl_ || !impl_->ready || !pixels || w <= 0 || h <= 0 || vp_w <= 0 || vp_h <= 0)
    return false;
  Impl& im = *impl_;
  Gl& gl = im.gl;
  const auto& passes = preset_.passes;
  const size_t n = passes.size();

  // Upload the frame (alpha forced opaque; the emulator leaves it 0).
  im.upload.resize(size_t(w) * size_t(h) * 4);
  for (size_t i = 0, count = size_t(w) * size_t(h); i < count; ++i) {
    const uint32_t c = pixels[i];
    im.upload[i * 4 + 0] = uint8_t(c >> 16);
    im.upload[i * 4 + 1] = uint8_t(c >> 8);
    im.upload[i * 4 + 2] = uint8_t(c);
    im.upload[i * 4 + 3] = 255;
  }
  // Overwrite the oldest frame of the ring (a size change restarts the
  // history with black frames).
  im.head = (im.head + 1) % im.frames.size();
  Target& cur = im.frames[im.head];
  gl.glActiveTexture(GL_TEXTURE0);
  gl.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  if (cur.w != w || cur.h != h) {
    const std::vector<uint8_t> black(im.upload.size(), 0);
    for (auto& f : im.frames) {
      gl.glBindTexture(GL_TEXTURE_2D, f.tex);
      gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      &f == &cur ? im.upload.data() : black.data());
      f.w = w;
      f.h = h;
      im.set_sampling(f.tex, passes[0], false);
    }
    gl.glBindTexture(GL_TEXTURE_2D, cur.tex);
  } else {
    gl.glBindTexture(GL_TEXTURE_2D, cur.tex);
    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                       im.upload.data());
  }
  im.source = cur;
  const bool src_mips = passes[0].mipmap_input;
  if (src_mips) gl.glGenerateMipmap(GL_TEXTURE_2D);
  im.set_sampling(im.source.tex, passes[0], src_mips);

  // (Re)allocate intermediate targets for the current sizes.
  im.outputs.resize(n > 0 ? n - 1 : 0);
  int in_w = w, in_h = h;
  for (size_t i = 0; i + 1 < n; ++i) {
    int ow = 0, oh = 0;
    pass_output_size(passes[i], false, in_w, in_h, vp_w, vp_h, ow, oh);
    Target& t = im.outputs[i];
    const GLenum format = passes[i].float_framebuffer ? GL_RGBA32F
                        : passes[i].srgb_framebuffer ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    if (t.w != ow || t.h != oh || t.format != format) {
      if (!t.tex) gl.glGenTextures(1, &t.tex);
      if (!t.fbo) gl.glGenFramebuffers(1, &t.fbo);
      gl.glBindTexture(GL_TEXTURE_2D, t.tex);
      gl.glTexImage2D(GL_TEXTURE_2D, 0, GLint(format), ow, oh, 0, GL_RGBA,
                      passes[i].float_framebuffer ? GL_FLOAT : GL_UNSIGNED_BYTE, nullptr);
      gl.glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
      gl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0);
      if (gl.glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        std::fprintf(stderr, "shader: framebuffer for pass %zu (%dx%d) incomplete\n", i, ow, oh);
        return false;
      }
      t.w = ow;
      t.h = oh;
      t.format = format;
    }
    in_w = ow;
    in_h = oh;
  }

  gl.glDisable(GL_BLEND);
  gl.glDisable(GL_DEPTH_TEST);
  gl.glDisable(GL_STENCIL_TEST);
  gl.glDisable(GL_SCISSOR_TEST);
  gl.glDisable(GL_CULL_FACE);
  if (im.vao) gl.glBindVertexArray(im.vao);
  gl.glBindBuffer(GL_ARRAY_BUFFER, im.vbo);
  for (GLuint a = 0; a < 3; ++a) gl.glEnableVertexAttribArray(a);
  constexpr GLsizei stride = 16 * sizeof(float);
  gl.glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, nullptr);
  gl.glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
                           reinterpret_cast<const void*>(12 * sizeof(float)));

  static const float mvp[16] = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, -1, -1, 0, 1};
  ++im.frame_count;

  for (size_t i = 0; i < n; ++i) {
    const bool last = i + 1 == n;
    const ShaderPass& pass = passes[i];
    Program& p = im.programs[i];
    const Target& input = i == 0 ? im.source : im.outputs[i - 1];
    if (i > 0) {
      const bool mips = pass.mipmap_input;
      gl.glActiveTexture(GL_TEXTURE0);
      gl.glBindTexture(GL_TEXTURE_2D, input.tex);
      if (mips) gl.glGenerateMipmap(GL_TEXTURE_2D);
      im.set_sampling(input.tex, pass, mips);
    }
    int ow = vp_w, oh = vp_h;
    if (last) {
      gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);
      gl.glDisable(GL_FRAMEBUFFER_SRGB);
      gl.glViewport(0, 0, drawable_w, drawable_h);
      gl.glClearColor(0, 0, 0, 1);
      gl.glClear(GL_COLOR_BUFFER_BIT);
      gl.glViewport(vp_x, vp_y, vp_w, vp_h);
    } else {
      const Target& out = im.outputs[i];
      ow = out.w;
      oh = out.h;
      gl.glBindFramebuffer(GL_FRAMEBUFFER, out.fbo);
      if (pass.srgb_framebuffer && !pass.float_framebuffer) gl.glEnable(GL_FRAMEBUFFER_SRGB);
      else gl.glDisable(GL_FRAMEBUFFER_SRGB);
      gl.glViewport(0, 0, ow, oh);
    }
    gl.glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride,
                             reinterpret_cast<const void*>((last ? 8 : 4) * sizeof(float)));
    gl.glUseProgram(p.prog);

    int unit = 0;
    auto bind_sampler = [&](const std::string& name, GLuint tex) {
      const GLint l = p.uniform(gl, name);
      if (l < 0) return;
      ++unit;
      gl.glActiveTexture(GLenum(GL_TEXTURE0 + unit));
      gl.glBindTexture(GL_TEXTURE_2D, tex);
      gl.glUniform1i(l, unit);
    };
    auto set2 = [&](const std::string& name, float x, float y) {
      const GLint l = p.uniform(gl, name);
      if (l >= 0) gl.glUniform2f(l, x, y);
    };
    auto bind_target = [&](const std::string& prefix, const Target& t) {
      bind_sampler(prefix + "Texture", t.tex);
      set2(prefix + "TextureSize", float(t.w), float(t.h));
      set2(prefix + "InputSize", float(t.w), float(t.h));
    };

    if (GLint l = p.uniform(gl, "MVPMatrix"); l >= 0) gl.glUniformMatrix4fv(l, 1, GL_FALSE, mvp);
    if (GLint l = p.uniform(gl, "FrameDirection"); l >= 0) gl.glUniform1i(l, 1);
    if (GLint l = p.uniform(gl, "FrameCount"); l >= 0) {
      const uint64_t fc = pass.frame_count_mod > 0
                              ? im.frame_count % uint64_t(pass.frame_count_mod) : im.frame_count;
      gl.glUniform1i(l, GLint(fc & 0x7fffffff));
    }
    set2("OutputSize", float(ow), float(oh));
    set2("TextureSize", float(input.w), float(input.h));
    set2("InputSize", float(input.w), float(input.h));
    if (GLint l = p.uniform(gl, "Texture"); l >= 0) gl.glUniform1i(l, 0);
    gl.glActiveTexture(GL_TEXTURE0);
    gl.glBindTexture(GL_TEXTURE_2D, input.tex);

    bind_target("Orig", im.source);
    // Previous original frames: Prev = 1 frame ago, Prev<k> = k+1 ago.
    if (im.frames.size() > 1)
      for (size_t k = 0; k < 7; ++k) {
        const Target& old = im.frames[(im.head + im.frames.size() - 1 - k) % im.frames.size()];
        bind_target(k == 0 ? "Prev" : "Prev" + std::to_string(k), old);
      }
    for (size_t k = 1; k <= i + 1; ++k)  // PassPrev<k>: output of pass i-k (-1 = source)
      bind_target("PassPrev" + std::to_string(k), i >= k ? im.outputs[i - k] : im.source);
    for (size_t k = 1; k <= i; ++k)      // Pass<k>: output of pass k-1
      bind_target("Pass" + std::to_string(k), im.outputs[k - 1]);
    for (size_t k = 0; k < i; ++k)
      if (!passes[k].alias.empty()) bind_target(passes[k].alias, im.outputs[k]);
    for (const Lut& lut : im.luts) bind_sampler(lut.name, lut.tex);
    for (const auto& [name, value] : p.params)
      if (GLint l = p.uniform(gl, name); l >= 0) gl.glUniform1f(l, value);

    gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
  gl.glUseProgram(0);
  gl.glActiveTexture(GL_TEXTURE0);
  if (std::getenv("RSR_SHADER_DEBUG")) {
    const GLenum e = gl.glGetError();
    if (e) std::fprintf(stderr, "shader: GL error 0x%x after frame %llu\n", e,
                        (unsigned long long)im.frame_count);
  }
  return true;
}

bool ShaderChain::read_rgb(std::vector<uint8_t>& rgb, int w, int h) {
  if (!impl_ || !impl_->ready || w <= 0 || h <= 0) return false;
  Gl& gl = impl_->gl;
  std::vector<uint8_t> rows(size_t(w) * size_t(h) * 3);
  gl.glBindFramebuffer(GL_FRAMEBUFFER, 0);
  gl.glPixelStorei(GL_PACK_ALIGNMENT, 1);
  gl.glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, rows.data());
  rgb.resize(rows.size());
  const size_t stride = size_t(w) * 3;
  for (int y = 0; y < h; ++y)  // GL rows are bottom-up
    std::memcpy(&rgb[size_t(y) * stride], &rows[size_t(h - 1 - y) * stride], stride);
  return true;
}

void letterbox(int drawable_w, int drawable_h, double aspect,
               int& x, int& y, int& w, int& h) {
  w = drawable_w;
  h = int(std::lround(drawable_w / aspect));
  if (h > drawable_h) {
    h = drawable_h;
    w = int(std::lround(drawable_h * aspect));
  }
  w = std::max(1, w);
  h = std::max(1, h);
  x = (drawable_w - w) / 2;
  y = (drawable_h - h) / 2;
}

}  // namespace present
