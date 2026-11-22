// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// Red Storm Rising port - SDL3 front end.
// Keyboard mapping and presentation follow fa18-interceptor-port's front end.
#include "amiga/machine.hpp"
#include "host/game_archive.hpp"
#include "launcher.hpp"
#include "present/shader_chain.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

int amiga_rawkey(SDL_Scancode sc) {
  switch (sc) {
    case SDL_SCANCODE_GRAVE: return 0x00;
    case SDL_SCANCODE_1: return 0x01; case SDL_SCANCODE_2: return 0x02;
    case SDL_SCANCODE_3: return 0x03; case SDL_SCANCODE_4: return 0x04;
    case SDL_SCANCODE_5: return 0x05; case SDL_SCANCODE_6: return 0x06;
    case SDL_SCANCODE_7: return 0x07; case SDL_SCANCODE_8: return 0x08;
    case SDL_SCANCODE_9: return 0x09; case SDL_SCANCODE_0: return 0x0A;
    case SDL_SCANCODE_MINUS: return 0x0B; case SDL_SCANCODE_EQUALS: return 0x0C;
    case SDL_SCANCODE_BACKSLASH: return 0x0D;
    case SDL_SCANCODE_Q: return 0x10; case SDL_SCANCODE_W: return 0x11;
    case SDL_SCANCODE_E: return 0x12; case SDL_SCANCODE_R: return 0x13;
    case SDL_SCANCODE_T: return 0x14; case SDL_SCANCODE_Y: return 0x15;
    case SDL_SCANCODE_U: return 0x16; case SDL_SCANCODE_I: return 0x17;
    case SDL_SCANCODE_O: return 0x18; case SDL_SCANCODE_P: return 0x19;
    case SDL_SCANCODE_LEFTBRACKET: return 0x1A; case SDL_SCANCODE_RIGHTBRACKET: return 0x1B;
    case SDL_SCANCODE_A: return 0x20; case SDL_SCANCODE_S: return 0x21;
    case SDL_SCANCODE_D: return 0x22; case SDL_SCANCODE_F: return 0x23;
    case SDL_SCANCODE_G: return 0x24; case SDL_SCANCODE_H: return 0x25;
    case SDL_SCANCODE_J: return 0x26; case SDL_SCANCODE_K: return 0x27;
    case SDL_SCANCODE_L: return 0x28; case SDL_SCANCODE_SEMICOLON: return 0x29;
    case SDL_SCANCODE_APOSTROPHE: return 0x2A;
    case SDL_SCANCODE_NONUSBACKSLASH: return 0x30;
    case SDL_SCANCODE_Z: return 0x31; case SDL_SCANCODE_X: return 0x32;
    case SDL_SCANCODE_C: return 0x33; case SDL_SCANCODE_V: return 0x34;
    case SDL_SCANCODE_B: return 0x35; case SDL_SCANCODE_N: return 0x36;
    case SDL_SCANCODE_M: return 0x37; case SDL_SCANCODE_COMMA: return 0x38;
    case SDL_SCANCODE_PERIOD: return 0x39; case SDL_SCANCODE_SLASH: return 0x3A;
    case SDL_SCANCODE_SPACE: return 0x40; case SDL_SCANCODE_BACKSPACE: return 0x41;
    case SDL_SCANCODE_TAB: return 0x42; case SDL_SCANCODE_KP_ENTER: return 0x43;
    case SDL_SCANCODE_RETURN: return 0x44; case SDL_SCANCODE_ESCAPE: return 0x45;
    case SDL_SCANCODE_DELETE: return 0x46; case SDL_SCANCODE_KP_MINUS: return 0x4A;
    case SDL_SCANCODE_UP: return 0x4C; case SDL_SCANCODE_DOWN: return 0x4D;
    case SDL_SCANCODE_RIGHT: return 0x4E; case SDL_SCANCODE_LEFT: return 0x4F;
    case SDL_SCANCODE_F1: return 0x50; case SDL_SCANCODE_F2: return 0x51;
    case SDL_SCANCODE_F3: return 0x52; case SDL_SCANCODE_F4: return 0x53;
    case SDL_SCANCODE_F5: return 0x54; case SDL_SCANCODE_F6: return 0x55;
    case SDL_SCANCODE_F7: return 0x56; case SDL_SCANCODE_F8: return 0x57;
    case SDL_SCANCODE_F9: return 0x58; case SDL_SCANCODE_F10: return 0x59;
    case SDL_SCANCODE_KP_0: return 0x0F; case SDL_SCANCODE_KP_1: return 0x1D;
    case SDL_SCANCODE_KP_2: return 0x1E; case SDL_SCANCODE_KP_3: return 0x1F;
    case SDL_SCANCODE_KP_4: return 0x2D; case SDL_SCANCODE_KP_5: return 0x2E;
    case SDL_SCANCODE_KP_6: return 0x2F; case SDL_SCANCODE_KP_7: return 0x3D;
    case SDL_SCANCODE_KP_8: return 0x3E; case SDL_SCANCODE_KP_9: return 0x3F;
    case SDL_SCANCODE_KP_PERIOD: return 0x3C; case SDL_SCANCODE_KP_DIVIDE: return 0x5C;
    case SDL_SCANCODE_KP_MULTIPLY: return 0x5D; case SDL_SCANCODE_KP_PLUS: return 0x5E;
    case SDL_SCANCODE_HOME: return 0x5F;  // HELP
    case SDL_SCANCODE_LSHIFT: return 0x60; case SDL_SCANCODE_RSHIFT: return 0x61;
    case SDL_SCANCODE_CAPSLOCK: return 0x62; case SDL_SCANCODE_LCTRL: return 0x63;
    case SDL_SCANCODE_LALT: return 0x64; case SDL_SCANCODE_RALT: return 0x65;
    case SDL_SCANCODE_LGUI: return 0x66; case SDL_SCANCODE_RGUI: return 0x67;
    default: return -1;
  }
}

// Scripted input for automated runs, one event per line:
//   <frame> key <SDL key name|hex rawkey> [down|up|tap]
//   <frame> mouse <dx> <dy>      <frame> lmb|rmb <0|1>      <frame> click
//   <frame> shot <file.ppm>      <frame> quit
//   <frame> set <module> <hex address|a4+N|a4-N> <1|2|4> <value>
//     (tests only: store a value in a game variable, to reach states that
//     take hours of play; addresses in the tools' layout). *a4+N+M: the
//     long at a4+N points to the variable, M bytes on (a record in the heap)
//   <frame> get <module> <address> <1|2|4>   (log the variable's value)
//   <frame> pointer   (log Intuition's pointer position, view pixels)
//   <frame> disks out|in   (tests: the volumes cannot / can be locked, as
//     with the game's disks out of the drives: its insert-disk requesters)
// <frame> may be +N (relative to the previous line).
struct ScriptEvent {
  int frame;
  std::string cmd;
  std::vector<std::string> args;
};

std::vector<ScriptEvent> load_script(const std::string& path) {
  std::vector<ScriptEvent> ev;
  std::ifstream in(path);
  std::string line;
  int last = 0;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream ss(line);
    std::string f;
    ScriptEvent e;
    ss >> f >> e.cmd;
    e.frame = f[0] == '+' ? last + std::atoi(f.c_str() + 1) : std::atoi(f.c_str());
    last = e.frame;
    std::string a;
    while (ss >> a) e.args.push_back(a);
    ev.push_back(e);
  }
  return ev;
}

// Bundled shader presets (data/shaders): $RSR_SHADER_DIR, next to an
// installed binary, the install prefix, then the source tree.
std::string find_bundled_shader(const std::string& relative) {
  namespace fs = std::filesystem;
  std::vector<fs::path> dirs;
  if (const char* env = std::getenv("RSR_SHADER_DIR"); env && *env) dirs.emplace_back(env);
  if (const char* base = SDL_GetBasePath()) {
    dirs.push_back(fs::path(base) / "shaders");  // next to the program (the Windows package)
    dirs.push_back(fs::path(base) / ".." / "share" / "redstormrising" / "shaders");
  }
  dirs.push_back(fs::path(RSR_DATADIR) / "shaders");
  dirs.emplace_back(RSR_SOURCE_SHADER_DIR);
  for (const auto& d : dirs) {
    std::error_code ec;
    const fs::path p = d / relative;
    if (fs::is_regular_file(p, ec)) return p.lexically_normal().string();
  }
  return {};
}

// 48 kHz stereo S16 = 192 bytes per ms.
constexpr int kAudioBytesPerMs = 192;
// The frame pacing keeps the audio queue this far above one frame's
// audio (read just after a frame added its own), so the sound stays about
// that far behind the picture and the device never finds it empty.
constexpr int kAudioMarginMs = 30;
constexpr int kAudioCushionBytes = 40 * kAudioBytesPerMs;
constexpr int kAudioMaxBytes = 150 * kAudioBytesPerMs;

class SdlHost : public amiga::Host {
public:
  // --crt-royale / --shader-preset: post-process every frame with a
  // libretro GLSL preset in an OpenGL 3 context on the window; any error
  // falls back to the SDL renderer.
  std::string shader_preset;
  std::unique_ptr<present::ShaderChain> shader;
  SDL_GLContext shader_ctx = nullptr;
  SDL_Window* win = nullptr;
  SDL_Renderer* ren = nullptr;
  SDL_Texture* tex = nullptr;
  SDL_AudioStream* audio_dev = nullptr;
  double pacing_drift_ms = 0;  // frame(): the frame time added for the sound card's clock
  amiga::Machine* machine = nullptr;
  bool fast = false, fullscreen = false, quit = false;
  bool no_present = false;  // --no-present: emulate without drawing to the window
  int frame_no = 0, quit_after = -1, shot_frame = -1;
  std::string shot_path;
  uint64_t next_frame = 0;
  float mouse_rem_x = 0, mouse_rem_y = 0;
  std::vector<ScriptEvent> script;
  size_t script_pos = 0;
  std::vector<std::pair<int, int>> pending_up;   // frame, rawkey
  std::vector<std::pair<int, int>> pending_btn;  // frame, button
  std::string pending_shot;

  void run_script() {
    for (size_t i = 0; i < pending_up.size();)
      if (pending_up[i].first <= frame_no) {
        machine->key_event(uint8_t(pending_up[i].second), false);
        pending_up.erase(pending_up.begin() + long(i));
      } else ++i;
    for (size_t i = 0; i < pending_btn.size();)
      if (pending_btn[i].first <= frame_no) {
        machine->mouse_button(pending_btn[i].second, false);
        pending_btn.erase(pending_btn.begin() + long(i));
      } else ++i;
    while (script_pos < script.size() && script[script_pos].frame <= frame_no) {
      const ScriptEvent& e = script[script_pos++];
      if (e.cmd == "key" && !e.args.empty()) {
        int rk = amiga_rawkey(SDL_GetScancodeFromName(e.args[0].c_str()));
        if (rk < 0) rk = int(std::strtol(e.args[0].c_str(), nullptr, 16));
        const std::string mode = e.args.size() > 1 ? e.args[1] : "tap";
        if (mode == "down") machine->key_event(uint8_t(rk), true);
        else if (mode == "up") machine->key_event(uint8_t(rk), false);
        else { machine->key_event(uint8_t(rk), true); pending_up.push_back({frame_no + 4, rk}); }
      } else if (e.cmd == "mouse" && e.args.size() >= 2) {
        machine->mouse_move(std::atoi(e.args[0].c_str()), std::atoi(e.args[1].c_str()));
      } else if ((e.cmd == "lmb" || e.cmd == "rmb") && !e.args.empty()) {
        machine->mouse_button(e.cmd == "lmb" ? 0 : 1, e.args[0] != "0");
      } else if (e.cmd == "click") {
        machine->mouse_button(0, true);
        pending_btn.push_back({frame_no + 4, 0});
      } else if (e.cmd == "shot" && !e.args.empty()) {
        pending_shot = e.args[0];
      } else if (e.cmd == "set" && e.args.size() >= 4) {
        machine->set_module_var(e.args[0], e.args[1], std::atoi(e.args[2].c_str()),
                                uint32_t(std::strtol(e.args[3].c_str(), nullptr, 0)));
      } else if (e.cmd == "get" && e.args.size() >= 3) {
        machine->set_module_var(e.args[0], e.args[1], std::atoi(e.args[2].c_str()), 0, false);
      } else if (e.cmd == "pointer") {
        int x = 0, y = 0;
        machine->pointer(x, y);
        machine->log("[script] frame %d pointer %d %d\n", frame_no, x, y);
      } else if (e.cmd == "disks" && !e.args.empty()) {
        machine->files.disks_out = e.args[0] == "out";
      } else if (e.cmd == "quit") {
        quit = true;
      }
    }
  }

  bool init(int scale) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
      fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
      return false;
    }
    const int w = amiga::FB_W * scale, h = amiga::FB_H * scale * 6 / 5;
    win = SDL_CreateWindow("Red Storm Rising", w, h,
                           SDL_WINDOW_RESIZABLE | (fullscreen ? SDL_WINDOW_FULLSCREEN : 0) |
                           (shader_preset.empty() ? 0 : SDL_WINDOW_OPENGL));
    if (!win) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return false; }
    ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) { fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); return false; }
    // NTSC lores pixels are slightly taller than wide; 320x240 approximates.
    SDL_SetRenderLogicalPresentation(ren, amiga::FB_W, amiga::FB_H * 6 / 5,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    // XRGB: the emulated framebuffer leaves the top byte 0.
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING,
                            amiga::FB_W, amiga::FB_H);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    if (!shader_preset.empty()) init_shader();
    // The game draws its own pointer (Intuition sprite); hide the host
    // cursor over the window. Motion is mapped to absolute game positions.
    SDL_HideCursor();
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "1024");
    const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, 48000};
    audio_dev = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (audio_dev) SDL_ResumeAudioStreamDevice(audio_dev);
    next_frame = SDL_GetPerformanceCounter();
    return true;
  }

  void init_shader() {
    auto chain = std::make_unique<present::ShaderChain>();
    std::string err;
    if (!chain->load(shader_preset, err)) {
      fprintf(stderr, "shader: %s; using normal output\n", err.c_str());
      return;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    shader_ctx = SDL_GL_CreateContext(win);
    if (!shader_ctx) {
      fprintf(stderr, "shader: no OpenGL 3 context (%s); using normal output\n", SDL_GetError());
      return;
    }
    SDL_GL_MakeCurrent(win, shader_ctx);
    SDL_GL_SetSwapInterval(0);
    if (!chain->init_gl([](const char* n) { return reinterpret_cast<void*>(SDL_GL_GetProcAddress(n)); },
                        err)) {
      fprintf(stderr, "shader: %s; using normal output\n", err.c_str());
      SDL_GL_DestroyContext(shader_ctx);
      shader_ctx = nullptr;
      return;
    }
    fprintf(stderr, "shader: %s (%zu passes)\n", shader_preset.c_str(), chain->pass_count());
    shader = std::move(chain);
  }

  // Draw the frame through the shader preset (320x240 aspect, letterboxed).
  bool present_shader(const uint32_t* fb) {
    if (!shader) return false;
    SDL_GL_MakeCurrent(win, shader_ctx);
    int dw = 0, dh = 0;
    SDL_GetWindowSizeInPixels(win, &dw, &dh);
    int vx = 0, vy = 0, vw = 0, vh = 0;
    present::letterbox(dw, dh, double(amiga::FB_W) / (amiga::FB_H * 6 / 5), vx, vy, vw, vh);
    static const bool timing = std::getenv("RSR_AUDIO_DEBUG") != nullptr;
    const uint64_t t0 = SDL_GetPerformanceCounter();
    if (!shader->render(fb, amiga::FB_W, amiga::FB_H, dw, dh, vx, vy, vw, vh)) {
      fprintf(stderr, "shader: rendering failed; using normal output\n");
      shader.reset();
      return false;
    }
    if (frame_no == shot_frame)
      if (const char* path = std::getenv("RSR_PRESENT_SHOT"); path && *path) {
        std::vector<uint8_t> rgb;
        if (shader->read_rgb(rgb, dw, dh))
          if (FILE* f = fopen(path, "wb")) {
            fprintf(f, "P6\n%d %d\n255\n", dw, dh);
            fwrite(rgb.data(), 1, rgb.size(), f);
            fclose(f);
          }
      }
    const uint64_t t1 = SDL_GetPerformanceCounter();
    SDL_GL_SwapWindow(win);
    if (timing) {
      static double render_ms = 0, swap_ms = 0;
      static int n = 0;
      const double f = 1000.0 / double(SDL_GetPerformanceFrequency());
      render_ms += double(t1 - t0) * f;
      swap_ms += double(SDL_GetPerformanceCounter() - t1) * f;
      if (++n == 60) {
        fprintf(stderr, "shader: %dx%d render %.1f ms swap %.1f ms (avg)\n", dw, dh,
                render_ms / n, swap_ms / n);
        render_ms = swap_ms = 0;
        n = 0;
      }
    }
    return true;
  }

  // Free GL objects while their context still exists (before SDL_Quit).
  void shutdown_shader() {
    if (!shader_ctx) return;
    SDL_GL_MakeCurrent(win, shader_ctx);
    shader.reset();
    SDL_GL_DestroyContext(shader_ctx);
    shader_ctx = nullptr;
  }

  void handle_events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_EVENT_QUIT: quit = true; break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
          const bool down = e.type == SDL_EVENT_KEY_DOWN;
          if (e.key.scancode == SDL_SCANCODE_F11 && down && !e.key.repeat) {
            const bool fs = (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN) != 0;
            SDL_SetWindowFullscreen(win, !fs);
            break;
          }
          if (e.key.scancode == SDL_SCANCODE_F12 && down) {
            SDL_SetWindowRelativeMouseMode(win, !SDL_GetWindowRelativeMouseMode(win));
            break;
          }
          if (e.key.repeat) break;
          const int rk = amiga_rawkey(e.key.scancode);
          if (rk >= 0) machine->key_event(uint8_t(rk), down);
          break;
        }
        case SDL_EVENT_MOUSE_MOTION: {
          if (SDL_GetWindowRelativeMouseMode(win)) {
            // Captured (F12): relative motion, window pixels -> lores pixels.
            int ww = 1, wh = 1;
            SDL_GetWindowSize(win, &ww, &wh);
            mouse_rem_x += e.motion.xrel * amiga::FB_W / float(ww);
            mouse_rem_y += e.motion.yrel * amiga::FB_H / float(wh);
            const int dx = int(mouse_rem_x), dy = int(mouse_rem_y);
            mouse_rem_x -= float(dx);
            mouse_rem_y -= float(dy);
            if (dx || dy) machine->mouse_move(dx, dy);
            break;
          }
          // Not captured: the game pointer follows the host cursor. Map the
          // window position into the letterboxed 320x240 picture, then to
          // the 320x200 screen, and move by the difference.
          float lx = 0, ly = 0;
          SDL_RenderCoordinatesFromWindow(ren, e.motion.x, e.motion.y, &lx, &ly);
          const int tx = std::clamp(int(lx), 0, amiga::FB_W - 1);
          const int ty = std::clamp(int(ly * amiga::FB_H / (amiga::FB_H * 6 / 5)), 0, amiga::FB_H - 1);
          int px = 0, py = 0;
          machine->pointer(px, py);  // includes movement still queued
          if (tx != px || ty != py) machine->mouse_move(tx - px, ty - py);
          break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
          const bool down = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
          if (e.button.button == SDL_BUTTON_LEFT) machine->mouse_button(0, down);
          if (e.button.button == SDL_BUTTON_RIGHT) machine->mouse_button(1, down);
          break;
        }
      }
    }
  }

  void frame(const uint32_t* fb) override {
    ++frame_no;
    if (!no_present && !present_shader(fb)) {
      SDL_UpdateTexture(tex, nullptr, fb, amiga::FB_W * 4);
      SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
      SDL_RenderClear(ren);
      SDL_RenderTexture(ren, tex, nullptr, nullptr);
      if (frame_no == shot_frame) save_presented(std::getenv("RSR_PRESENT_SHOT"));
      SDL_RenderPresent(ren);
    }
    if (frame_no == shot_frame) save_ppm(fb, shot_path);
    if (!pending_shot.empty()) { save_ppm(fb, pending_shot); pending_shot.clear(); }
    if (quit_after >= 0 && frame_no >= quit_after) quit = true;
    handle_events();
    run_script();
    if (quit) throw amiga::QuitRequested{};
    // Pacing at the field rate (50 Hz PAL, 60 Hz NTSC). The sound card is
    // the clock: the emulation makes 960 or 800 samples a frame, and the host timer and the card never run at
    // exactly the same rate, so the queue would drain (gaps) or fill up to
    // kAudioMaxBytes (the sound far behind the picture). Each frame is
    // stretched or shortened a little by the queue's distance from its
    // target (at most 0.5 ms, 3%).
    const uint64_t freq = SDL_GetPerformanceFrequency();
    next_frame += uint64_t(double(freq) / machine->field_rate());
    if (audio_dev && !fast) {
      const double target_ms = kAudioMarginMs + 1000.0 / machine->field_rate();
      const double error_ms = double(SDL_GetAudioStreamQueued(audio_dev)) / kAudioBytesPerMs - target_ms;
      // The integral learns the clocks' steady difference, the rest
      // corrects what is left.
      pacing_drift_ms = std::clamp(pacing_drift_ms + error_ms * 0.0002, -0.5, 0.5);
      const double adjust_ms = std::clamp(pacing_drift_ms + error_ms * 0.02, -0.5, 0.5);
      next_frame += uint64_t(int64_t(adjust_ms * double(freq) / 1000.0));
    }
    const uint64_t now = SDL_GetPerformanceCounter();
    if (fast || int64_t(next_frame - now) < -int64_t(freq / 10)) next_frame = now;
    while (!fast) {
      const int64_t left = int64_t(next_frame - SDL_GetPerformanceCounter());
      if (left <= 0) break;
      const int64_t left_us = left * 1000000 / int64_t(freq);
      if (left_us > 2000) SDL_Delay(uint32_t(left_us / 1000 - 1));
    }
  }

  // Test aid (RSR_PRESENT_SHOT=FILE with --screenshot N): what the SDL
  // renderer put in the window, as PPM.
  void save_presented(const char* path) {
    if (!path || !*path) return;
    SDL_Surface* s = SDL_RenderReadPixels(ren, nullptr);
    SDL_Surface* rgb = s ? SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGB24) : nullptr;
    if (rgb)
      if (FILE* f = fopen(path, "wb")) {
        fprintf(f, "P6\n%d %d\n255\n", rgb->w, rgb->h);
        for (int y = 0; y < rgb->h; ++y)
          fwrite(static_cast<const uint8_t*>(rgb->pixels) + size_t(y) * size_t(rgb->pitch), 1,
                 size_t(rgb->w) * 3, f);
        fclose(f);
      }
    SDL_DestroySurface(rgb);
    SDL_DestroySurface(s);
  }

  void save_ppm(const uint32_t* fb, const std::string& path) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", amiga::FB_W, amiga::FB_H);
    for (int i = 0; i < amiga::FB_W * amiga::FB_H; ++i) {
      const uint8_t px[3] = {uint8_t(fb[i] >> 16), uint8_t(fb[i] >> 8), uint8_t(fb[i])};
      fwrite(px, 1, 3, f);
    }
    fclose(f);
  }

  FILE* wav = nullptr;
  uint32_t wav_bytes = 0;
  void open_wav(const std::string& path) {
    wav = fopen(path.c_str(), "wb");
    if (wav) { uint8_t hdr[44] = {}; fwrite(hdr, 1, 44, wav); }
  }
  void close_wav() {
    if (!wav) return;
    auto le32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) fputc(int(v >> (8 * i)) & 0xFF, wav); };
    auto le16 = [&](uint16_t v) { fputc(v & 0xFF, wav); fputc(v >> 8, wav); };
    fseek(wav, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, wav); le32(36 + wav_bytes); fwrite("WAVEfmt ", 1, 8, wav);
    le32(16); le16(1); le16(2); le32(48000); le32(48000 * 4); le16(4); le16(16);
    fwrite("data", 1, 4, wav); le32(wav_bytes);
    fclose(wav);
    wav = nullptr;
  }

  void audio(const int16_t* samples, size_t frames) override {
    if (wav) { fwrite(samples, 4, frames, wav); wav_bytes += uint32_t(frames * 4); }
    if (!audio_dev || fast) return;
    const int queued = SDL_GetAudioStreamQueued(audio_dev);
    static const bool debug = std::getenv("RSR_AUDIO_DEBUG") != nullptr;
    if (debug) {
      static uint64_t t0 = SDL_GetTicks();
      static int n = 0, drops = 0, minq = 1 << 30, maxq = 0;
      ++n;
      minq = std::min(minq, queued);
      maxq = std::max(maxq, queued);
      if (queued > kAudioMaxBytes) ++drops;
      if (SDL_GetTicks() - t0 >= 1000) {
        fprintf(stderr, "audio: %d frames/s queue %d..%d ms drops %d\n", n, minq / 192,
                maxq / 192, drops);
        t0 = SDL_GetTicks();
        n = drops = maxq = 0;
        minq = 1 << 30;
      }
    }
    // The device pulls ~1024-frame (21 ms) chunks while each video frame
    // adds 16.7 ms: keep a cushion (the pacing in frame() holds the queue
    // there), or the queue runs dry and the output crackles. Re-prime after
    // an underrun; drop only far above it.
    if (queued == 0) {
      static const std::vector<uint8_t> silence(kAudioCushionBytes, 0);
      SDL_PutAudioStreamData(audio_dev, silence.data(), int(silence.size()));
    }
    if (queued > kAudioMaxBytes) return;
    SDL_PutAudioStreamData(audio_dev, samples, int(frames * 4));
  }
};

// The user's folder for the game: the saves, and the game's files when
// they come from the archive. Windows: %APPDATA%\\RedStormRising; else
// $XDG_DATA_HOME/redstormrising (~/.local/share/redstormrising).
std::string user_dir() {
#ifdef _WIN32
  std::string dir = ".";
  if (char* pref = SDL_GetPrefPath("", "RedStormRising")) {
    dir = pref;
    SDL_free(pref);
  }
  return dir;
#else
  const char* xdg = getenv("XDG_DATA_HOME");
  const char* home = getenv("HOME");
  if (xdg && *xdg) return std::string(xdg) + "/redstormrising";
  if (home) return std::string(home) + "/.local/share/redstormrising";
  return ".";
#endif
}

void usage() {
  fprintf(stderr,
          "Red Storm Rising port " RSR_VERSION "\n"
          "usage: rsr [options] [GAME...]\n"
          "  GAME             the WHDLoad install archive (RedStormRising_v1.0_1603.lha), or\n"
          "                   both disks of v843.02 (two .adf), their files extracted once\n"
          "                   into the user's folder; or the install's data/ directory.\n"
          "                   Default: a `data` folder, an .lha or the .adf disks next to\n"
          "                   the program, or the files extracted before.\n"
          "  --save DIR       save directory (default $XDG_DATA_HOME/redstormrising;\n"
          "                   on Windows %%APPDATA%%\\RedStormRising)\n"
          "  --scale N        window scale (default 3)\n"
          "  --fullscreen     start in fullscreen (F11 toggles)\n"
          "  --crt-royale     CRT monitor look (bundled crt-royale shader; OpenGL 3)\n"
          "  --crt-lottes-fast  lightweight CRT look (optimised crt-lottes)\n"
          "  --crt-guest      CRT look by guest(r) (crt-guest-dr-venom, 11 passes)\n"
          "  --crt-guest-fast  lighter crt-guest-dr-venom (5 passes)\n"
          "  --shader-preset FILE  post-process with a libretro GLSL preset (.glslp)\n"
          "  --version        print the version\n"
          "  --launcher       show the launch screen (the default without options)\n"
          "  --no-launcher    start the game at once\n"
          "  --speed X        CPU speed multiplier (default 1 = stock A500)\n"
          "  --frames N       quit after N frames\n"
          "  --screenshot N FILE  save frame N as PPM\n"
          "  --fast           run unthrottled\n"
          "  --no-present     do not draw to the window (benchmarks, test runs)\n"
          "  --script FILE    scripted input (see src/main.cpp)\n"
          "  --wav FILE       record the audio output\n"
          "  --verbose        log OS calls\n"
          "  --protection     keep the manual-protection question\n"
          "  --original-input read input as the original did: quick clicks and quickly\n"
          "                   typed keys can be lost (docs/known-issues.md)\n"
          "  --alternative-color  a darker grid on the weapon control display (F2)\n"
          "  --detailed-map   the naval ports marked on the strategic map\n"
          "  --pal            a European (PAL, 50 Hz) Amiga (the default)\n"
          "  --ntsc           an American (NTSC, 60 Hz) Amiga: pictures run 20%% faster\n"
          "  --det-timing     deterministic timing (for comparing runs)\n"
          "  --ram-hash N     log a hash of game memory every N frames\n"
          "  --av-hash N      log digests of the picture and sound every N frames\n"
          "keys: F11 fullscreen, F12 capture/release the mouse\n");
}

}  // namespace

int main(int argc, char** argv) {
  std::string data_dir, save_dir;
  std::vector<std::string> game;  // the GAME arguments: a directory, an .lha, or the disks (.adf)
  int scale = 3;
  double speed = 1.0;
  bool det_timing = false;
  std::string crt;  // bundled preset chosen by --crt-<name>
  // The launch screen: by default only without options (as from a
  // double-click or run.sh / run.bat); --launcher, --no-launcher.
  int launch_screen = -1;
  for (int i = 1; i < argc; ++i)
    if (argv[i][0] == '-' && std::strcmp(argv[i], "--launcher") != 0) launch_screen = launch_screen == 1 ? 1 : 0;
  SdlHost host;
  amiga::Machine machine;
  machine.pal = true;  // the default: a European Amiga
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--save" && i + 1 < argc) save_dir = argv[++i];
    else if (a == "--scale" && i + 1 < argc) scale = std::atoi(argv[++i]);
    else if (a == "--fullscreen") host.fullscreen = true;
    else if (a == "--crt-royale") crt = "crt-royale";
    else if (a == "--crt-lottes-fast") crt = "crt-lottes-fast";
    else if (a == "--crt-guest") crt = "crt-guest-dr-venom";
    else if (a == "--crt-guest-fast") crt = "crt-guest-dr-venom-fast";
    else if (a == "--shader-preset" && i + 1 < argc) host.shader_preset = argv[++i];
    else if (a == "--speed" && i + 1 < argc) speed = std::atof(argv[++i]);
    else if (a == "--frames" && i + 1 < argc) host.quit_after = std::atoi(argv[++i]);
    else if (a == "--screenshot" && i + 2 < argc) {
      host.shot_frame = std::atoi(argv[++i]);
      host.shot_path = argv[++i];
    }
    else if (a == "--fast") host.fast = true;
    else if (a == "--no-present") host.no_present = true;
    else if (a == "--wav" && i + 1 < argc) host.open_wav(argv[++i]);
    else if (a == "--script" && i + 1 < argc) host.script = load_script(argv[++i]);
    else if (a == "--verbose") machine.verbose = true;
    else if (a == "--protection") machine.bypass_protection = false;
    else if (a == "--reference-terrain") machine.reference_terrain = true;
    else if (a == "--original-input") machine.original_input = true;
    else if (a == "--alternative-color") machine.alternative_color = true;
    else if (a == "--detailed-map") machine.detailed_map = true;
    else if (a == "--pal") machine.pal = true;
    else if (a == "--ntsc") machine.pal = false;
    else if (a == "--det-timing") det_timing = true;
    else if (a == "--ram-hash" && i + 1 < argc) machine.ram_hash_every = std::atoi(argv[++i]);
    else if (a == "--av-hash" && i + 1 < argc) machine.av_hash_every = std::atoi(argv[++i]);
    else if (a == "-h" || a == "--help") { usage(); return 0; }
    else if (a == "--version") { printf("Red Storm Rising port %s\n", RSR_VERSION); return 0; }
    else if (a == "--launcher") launch_screen = 1;
    else if (a == "--no-launcher") launch_screen = 0;
    else if (!a.empty() && a[0] == '-') { usage(); return 2; }
    else game.push_back(a);
  }
  std::string data_error;  // why there are no game files (shown by the launch screen)
  // The game's files: a directory, or the WHDLoad install archive (its
  // files extracted once into the user's folder); by default a `data`
  // folder next to the program, the files extracted before, an archive
  // next to the program, the source tree's copy.
  {
    namespace fs = std::filesystem;
    const fs::path extracted = fs::path(user_dir()) / "data";
    // (An archive given on the command line is always read and checked;
    // one found next to the program only when nothing was extracted yet.)
    std::string archive_error;  // why the files could not be used (for the launch screen)
    auto from_archive = [&](const std::vector<fs::path>& files, bool given) {
      if (!given && host::has_game_files(extracted.string())) return true;
      std::vector<std::string> paths;
      std::string names;
      for (const fs::path& f : files) {
        paths.push_back(f.string());
        names += (names.empty() ? "" : " + ") + f.filename().string();
      }
      fprintf(stderr, "extracting the game's files from %s to %s\n", names.c_str(), extracted.string().c_str());
      std::string err;
      if (host::extract_game_files(paths, extracted.string(), err)) return true;
      fprintf(stderr, "%s\n", err.c_str());
      archive_error = err;  // shown with the files' names only
      for (const fs::path& f : files)
        for (size_t at; (at = archive_error.find(f.string())) != std::string::npos;)
          archive_error.replace(at, f.string().size(), f.filename().string());
      return false;
    };
    std::error_code ec;
    if (game.size() == 1 && fs::is_directory(game[0], ec)) {
      data_dir = game[0];
    } else if (!game.empty()) {
      if (from_archive(std::vector<fs::path>(game.begin(), game.end()), true)) {
        data_dir = extracted.string();
      } else {
        if (launch_screen == 0) return 1;
        data_error = archive_error;
      }
    } else {
      const char* base = SDL_GetBasePath();
      const fs::path here = base ? fs::path(base) : fs::path(".");
      fs::path lha;
      std::vector<fs::path> adfs;
      for (const auto& e : fs::directory_iterator(here, ec)) {
        std::string ext = e.path().extension().string();
        for (char& c : ext) c = char(std::tolower(uint8_t(c)));
        if (!e.is_regular_file(ec)) continue;
        if (ext == ".lha") lha = e.path();
        else if (ext == ".adf") adfs.push_back(e.path());
      }
      const std::vector<fs::path> archive = lha.empty() ? adfs : std::vector<fs::path>{lha};
      if (fs::is_directory(here / "data", ec)) data_dir = (here / "data").string();
      else if (host::has_game_files(extracted.string())) data_dir = extracted.string();
      else if (!archive.empty()) {
        if (from_archive(archive, false)) data_dir = extracted.string();
        else if (launch_screen == 0) return 1;
        else data_error = archive_error;
      } else if (fs::is_directory(RSR_SOURCE_DATA_DIR, ec)) {
        data_dir = RSR_SOURCE_DATA_DIR;
      }
    }
    if (data_dir.empty() && data_error.empty()) {
      data_error = "No game files: put the WHDLoad .lha or both v843.02 disks (.adf) next to the program.";
      if (launch_screen == 0) {
        fprintf(stderr,
                "no game files: put the WHDLoad install archive (RedStormRising_v1.0_1603.lha) or both\n"
                "disks of v843.02 (.adf) next to the program, or give them (or the install's data/\n"
                "directory) on the command line\n\n");
        usage();
        return 2;
      }
    }
  }
  if (launch_screen != 0) {
    const std::string settings = (std::filesystem::path(user_dir()) / "launcher.cfg").string();
    std::error_code ec;
    std::filesystem::create_directories(user_dir(), ec);
    launcher::Settings ls = launcher::load(settings);
    ls.error = data_error;
    if (!launcher::run(ls)) return 0;
    launcher::save(ls, settings);
    crt = ls.crt;
    host.fullscreen = ls.fullscreen;
    machine.original_input = ls.original_input;
    machine.alternative_color = ls.alternative_color;
    machine.detailed_map = ls.detailed_map;
  }
  if (!crt.empty()) {
    if (!host.shader_preset.empty()) {
      fprintf(stderr, "--%s and --shader-preset select different shaders; choose one\n",
              crt.c_str());
      return 2;
    }
    host.shader_preset = find_bundled_shader("libretro/crt/" + crt + ".glslp");
    if (host.shader_preset.empty())
      fprintf(stderr, "--%s: bundled shader not found (set RSR_SHADER_DIR); "
                      "using normal output\n", crt.c_str());
  }
  if (save_dir.empty()) {
#ifdef _WIN32
    // %APPDATA%\RedStormRising
    if (char* pref = SDL_GetPrefPath("", "RedStormRising")) {
      save_dir = pref;
      SDL_free(pref);
    } else {
      save_dir = ".";
    }
#else
    const char* xdg = getenv("XDG_DATA_HOME");
    const char* home = getenv("HOME");
    if (xdg && *xdg) save_dir = std::string(xdg) + "/redstormrising";
    else if (home) save_dir = std::string(home) + "/.local/share/redstormrising";
    else save_dir = ".";
#endif
  }
  std::error_code ec;
  std::filesystem::create_directories(save_dir, ec);
  machine.save_dir = save_dir;
  machine.set_cpu_speed(speed);
  if (det_timing) machine.set_det_timing(true);
  if (!machine.boot(data_dir)) return 1;
  host.machine = &machine;
  if (!host.init(scale)) return 1;
  machine.set_host(&host);
  const int rc = machine.run();
  machine.write_data_writes();
  host.close_wav();
  host.shutdown_shader();
  SDL_Quit();
  return rc;
}
