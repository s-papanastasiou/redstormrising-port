// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// Copied from fa18-interceptor-port src/present (c586ff7).
#include "present/shader_preset.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace present {

namespace {

std::string trim(const std::string& s) {
  const size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return {};
  const size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

// `key = value` lines; values may be quoted; '#' starts a comment outside quotes.
bool parse_lines(const std::string& text, std::map<std::string, std::string>& kv,
                 std::string& err) {
  std::istringstream in(text);
  std::string line;
  int line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i) {
      if (line[i] == '"') quoted = !quoted;
      else if (line[i] == '#' && !quoted) { line.resize(i); break; }
    }
    line = trim(line);
    if (line.empty()) continue;
    const size_t eq = line.find('=');
    if (eq == std::string::npos) {
      err = "line " + std::to_string(line_no) + ": expected key = value";
      return false;
    }
    std::string key = trim(line.substr(0, eq)), value = trim(line.substr(eq + 1));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
      value = value.substr(1, value.size() - 2);
    kv[key] = value;
  }
  return true;
}

bool to_bool(const std::string& v) { return v == "true" || v == "1"; }

ScaleType to_scale_type(const std::string& v, bool& ok) {
  ok = true;
  if (v == "source") return ScaleType::Source;
  if (v == "viewport") return ScaleType::Viewport;
  if (v == "absolute") return ScaleType::Absolute;
  ok = false;
  return ScaleType::Source;
}

std::string resolve(const std::string& base_dir, const std::string& p) {
  namespace fs = std::filesystem;
  fs::path path(p);
  if (path.is_relative() && !base_dir.empty()) path = fs::path(base_dir) / path;
  return path.lexically_normal().string();
}

}  // namespace

bool parse_preset(const std::string& text, const std::string& base_dir,
                  ShaderPreset& out, std::string& err) {
  std::map<std::string, std::string> kv;
  if (!parse_lines(text, kv, err)) return false;
  auto get = [&](const std::string& k) -> const std::string* {
    auto it = kv.find(k);
    return it == kv.end() ? nullptr : &it->second;
  };
  const std::string* count = get("shaders");
  if (!count) { err = "missing 'shaders'"; return false; }
  const int n = std::atoi(count->c_str());
  if (n <= 0 || n > 64) { err = "invalid 'shaders' count"; return false; }
  out = {};
  for (int i = 0; i < n; ++i) {
    const std::string s = std::to_string(i);
    ShaderPass p;
    const std::string* path = get("shader" + s);
    if (!path) { err = "missing shader" + s; return false; }
    p.path = resolve(base_dir, *path);
    if (auto v = get("alias" + s)) p.alias = *v;
    if (auto v = get("filter_linear" + s)) p.filter_linear = to_bool(*v);
    if (auto v = get("mipmap_input" + s)) p.mipmap_input = to_bool(*v);
    if (auto v = get("wrap_mode" + s)) p.wrap_mode = *v;
    if (auto v = get("texture_wrap_mode" + s)) p.wrap_mode = *v;
    if (auto v = get("srgb_framebuffer" + s)) p.srgb_framebuffer = to_bool(*v);
    if (auto v = get("float_framebuffer" + s)) p.float_framebuffer = to_bool(*v);
    if (auto v = get("frame_count_mod" + s)) p.frame_count_mod = std::atoi(v->c_str());
    bool ok = true;
    if (auto v = get("scale_type" + s)) {
      p.scale_type_x = p.scale_type_y = to_scale_type(*v, ok);
      p.scale_given = true;
    }
    if (auto v = get("scale_type_x" + s)) {
      p.scale_type_x = to_scale_type(*v, ok);
      p.scale_given = true;
    }
    if (!ok) { err = "invalid scale_type for shader" + s; return false; }
    if (auto v = get("scale_type_y" + s)) {
      p.scale_type_y = to_scale_type(*v, ok);
      p.scale_given = true;
    }
    if (!ok) { err = "invalid scale_type for shader" + s; return false; }
    if (auto v = get("scale" + s)) p.scale_x = p.scale_y = float(std::atof(v->c_str()));
    if (auto v = get("scale_x" + s)) p.scale_x = float(std::atof(v->c_str()));
    if (auto v = get("scale_y" + s)) p.scale_y = float(std::atof(v->c_str()));
    out.passes.push_back(p);
  }
  if (auto list = get("textures")) {
    std::istringstream names(*list);
    std::string name;
    while (std::getline(names, name, ';')) {
      name = trim(name);
      if (name.empty()) continue;
      ShaderTexture t;
      t.name = name;
      const std::string* path = get(name);
      if (!path) { err = "missing path for texture " + name; return false; }
      t.path = resolve(base_dir, *path);
      if (auto v = get(name + "_linear")) t.linear = to_bool(*v);
      if (auto v = get(name + "_mipmap")) t.mipmap = to_bool(*v);
      if (auto v = get(name + "_wrap_mode")) t.wrap_mode = *v;
      out.textures.push_back(t);
    }
  }
  if (auto list = get("parameters")) {
    std::istringstream names(*list);
    std::string name;
    while (std::getline(names, name, ';')) {
      name = trim(name);
      if (auto v = get(name)) out.parameters[name] = float(std::atof(v->c_str()));
    }
  }
  return true;
}

bool load_preset(const std::string& path, ShaderPreset& out, std::string& err) {
  std::ifstream in(path);
  if (!in) { err = "cannot open " + path; return false; }
  std::stringstream ss;
  ss << in.rdbuf();
  const std::string dir = std::filesystem::path(path).parent_path().string();
  if (!parse_preset(ss.str(), dir, out, err)) {
    err = path + ": " + err;
    return false;
  }
  return true;
}

void pass_output_size(const ShaderPass& pass, bool last, int in_w, int in_h,
                      int vp_w, int vp_h, int& out_w, int& out_h) {
  if (last && !pass.scale_given) {
    out_w = vp_w;
    out_h = vp_h;
    return;
  }
  auto axis = [](ScaleType t, float scale, int in, int vp) {
    float v = 0.f;
    switch (t) {
      case ScaleType::Source: v = float(in) * scale; break;
      case ScaleType::Viewport: v = float(vp) * scale; break;
      case ScaleType::Absolute: v = scale; break;
    }
    return std::max(1, int(std::lround(v)));
  };
  out_w = axis(pass.scale_type_x, pass.scale_x, in_w, vp_w);
  out_h = axis(pass.scale_type_y, pass.scale_y, in_h, vp_h);
}

}  // namespace present
