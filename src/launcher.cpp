// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The launch screen (launcher.hpp). A 320x240 logical screen, scaled to
// the window like the game's, in SDL's built-in 8x8 font.
#include "launcher.hpp"

#include <SDL3/SDL.h>

#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace launcher {

namespace {

struct Crt {
  const char* preset;  // the bundled preset (main.cpp: data/shaders/libretro/crt/<preset>.glslp)
  const char* label;
  const char* about;
};
const Crt kCrts[] = {
    {"", "Off", "The Amiga's pixels as they are."},
    {"crt-lottes-fast", "Lottes (fast)", "Light: scanlines and a soft glow."},
    {"crt-guest-dr-venom-fast", "Guest (fast)", "Detailed CRT look, lighter version."},
    {"crt-guest-dr-venom", "Guest", "Detailed CRT look (11 passes)."},
    {"crt-royale", "Royale", "Richest look: needs a fast GPU."},
};
constexpr int kCrtCount = int(sizeof(kCrts) / sizeof(kCrts[0]));

constexpr int kW = 320, kH = 240;
constexpr float kChar = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;  // 8

struct Colour {
  Uint8 r, g, b;
};
const Colour kBack{0, 0, 0}, kTitle{0xff, 0x22, 0x22}, kText{0xdd, 0xdd, 0xdd}, kDim{0x88, 0x88, 0x88},
    kValue{0x55, 0xcc, 0xff}, kHighlight{0x10, 0x30, 0x58}, kWarn{0xff, 0xdd, 0x44}, kButton{0x30, 0x30, 0x30};

void colour(SDL_Renderer* r, Colour c) { SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255); }

void text(SDL_Renderer* r, float x, float y, const std::string& s, Colour c, float scale = 1) {
  colour(r, c);
  SDL_SetRenderScale(r, scale, scale);
  SDL_RenderDebugText(r, x / scale, y / scale, s.c_str());
  SDL_SetRenderScale(r, 1, 1);
}
void centred(SDL_Renderer* r, float y, const std::string& s, Colour c, float scale = 1) {
  text(r, (kW - float(s.size()) * kChar * scale) / 2, y, s, c, scale);
}

bool inside(const SDL_FRect& f, float x, float y) { return x >= f.x && x < f.x + f.w && y >= f.y && y < f.y + f.h; }

// The rows the player moves between.
enum Row { kRowCrt, kRowDisplay, kRowInput, kRowColor, kRowStart, kRowQuit };

}  // namespace

Settings load(const std::string& path) {
  Settings s;
  s.crt = "";
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
    if (key == "crt") s.crt = value;
    else if (key == "fullscreen") s.fullscreen = value != "0";
    else if (key == "input") s.original_input = value == "original";
    else if (key == "color") s.alternative_color = value == "alternative";
  }
  return s;
}

void save(const Settings& s, const std::string& path) {
  std::ofstream out(path);
  out << "crt=" << s.crt << "\nfullscreen=" << (s.fullscreen ? 1 : 0)
      << "\ninput=" << (s.original_input ? "original" : "responsive")
      << "\ncolor=" << (s.alternative_color ? "alternative" : "original") << "\n";
}

bool run(Settings& s) {
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return true;  // no video: start as before
  SDL_Window* win = SDL_CreateWindow("Red Storm Rising", kW * 3, kH * 3, SDL_WINDOW_RESIZABLE);
  SDL_Renderer* ren = win ? SDL_CreateRenderer(win, nullptr) : nullptr;
  if (!ren) {
    if (win) SDL_DestroyWindow(win);
    return true;
  }
  SDL_SetRenderLogicalPresentation(ren, kW, kH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
  SDL_SetRenderVSync(ren, 1);

  int crt = 0;
  for (int i = 0; i < kCrtCount; ++i)
    if (s.crt == kCrts[i].preset) crt = i;
  const bool can_start = s.error.empty();
  int row = can_start ? kRowStart : kRowQuit;

  // Where things are, for the mouse.
  const SDL_FRect start_button{70, 170, 80, 18}, quit_button{170, 170, 80, 18};
  const SDL_FRect crt_row{10, 70, 300, 16}, display_row{10, 89, 300, 16}, input_row{10, 108, 300, 16},
      color_row{10, 127, 300, 16};

  bool result = false, done = false;
  auto activate = [&](int r, int step) {
    switch (r) {
      case kRowCrt: crt = (crt + step + kCrtCount) % kCrtCount; break;
      case kRowDisplay: s.fullscreen = !s.fullscreen; break;
      case kRowInput: s.original_input = !s.original_input; break;
      case kRowColor: s.alternative_color = !s.alternative_color; break;
      case kRowStart:
        if (can_start) result = done = true;
        break;
      case kRowQuit: done = true; break;
    }
  };
  auto rows = [&] { return can_start ? std::vector<int>{kRowCrt, kRowDisplay, kRowInput, kRowColor, kRowStart, kRowQuit}
                                     : std::vector<int>{kRowCrt, kRowDisplay, kRowInput, kRowColor, kRowQuit}; };
  auto move = [&](int step) {
    const std::vector<int> rs = rows();
    size_t at = 0;
    while (at < rs.size() && rs[at] != row) ++at;
    row = rs[(at + rs.size() + size_t(step + int(rs.size()))) % rs.size()];
  };

  // RSR_LAUNCHER_KEYS=right,down,return (tests): key presses fed in first.
  if (const char* keys = SDL_getenv("RSR_LAUNCHER_KEYS"); keys && *keys) {
    std::string list = keys;
    for (size_t at = 0; at <= list.size();) {
      size_t end = list.find(',', at);
      if (end == std::string::npos) end = list.size();
      SDL_Event k{};
      k.type = SDL_EVENT_KEY_DOWN;
      k.key.key = SDL_GetKeyFromName(list.substr(at, end - at).c_str());
      k.key.down = true;
      SDL_PushEvent(&k);
      at = end + 1;
    }
  }
  while (!done) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      SDL_ConvertEventToRenderCoordinates(ren, &e);
      switch (e.type) {
        case SDL_EVENT_QUIT: done = true; break;
        case SDL_EVENT_KEY_DOWN:
          switch (e.key.key) {
            case SDLK_ESCAPE: done = true; break;
            case SDLK_UP: move(-1); break;
            case SDLK_DOWN: case SDLK_TAB: move(1); break;
            case SDLK_LEFT:
              if (row == kRowStart) row = kRowQuit;
              else if (row == kRowQuit && can_start) row = kRowStart;
              else activate(row, -1);
              break;
            case SDLK_RIGHT:
              if (row == kRowStart) row = kRowQuit;
              else if (row == kRowQuit && can_start) row = kRowStart;
              else activate(row, 1);
              break;
            case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: activate(row, 1); break;
            default: break;
          }
          break;
        case SDL_EVENT_MOUSE_MOTION: {
          const float x = e.motion.x, y = e.motion.y;
          if (inside(crt_row, x, y)) row = kRowCrt;
          else if (inside(display_row, x, y)) row = kRowDisplay;
          else if (inside(input_row, x, y)) row = kRowInput;
          else if (inside(color_row, x, y)) row = kRowColor;
          else if (can_start && inside(start_button, x, y)) row = kRowStart;
          else if (inside(quit_button, x, y)) row = kRowQuit;
          break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
          const float x = e.button.x, y = e.button.y;
          const int step = e.button.button == SDL_BUTTON_RIGHT ? -1 : 1;
          if (inside(crt_row, x, y)) activate(kRowCrt, step);
          else if (inside(display_row, x, y)) activate(kRowDisplay, step);
          else if (inside(input_row, x, y)) activate(kRowInput, step);
          else if (inside(color_row, x, y)) activate(kRowColor, step);
          else if (can_start && inside(start_button, x, y)) activate(kRowStart, 1);
          else if (inside(quit_button, x, y)) activate(kRowQuit, 1);
          break;
        }
        default: break;
      }
    }

    colour(ren, kBack);
    SDL_RenderClear(ren);
    centred(ren, 22, "RED STORM RISING", kTitle, 2);
    centred(ren, 46, "MicroProse, 1990 - the Amiga game", kDim);
    text(ren, kW - 4 - float(std::strlen("v" RSR_VERSION)) * kChar, 4, "v" RSR_VERSION, kDim);
    // The options.
    auto option = [&](int r, const SDL_FRect& f, float y, const char* label, const std::string& value) {
      if (row == r) {
        colour(ren, kHighlight);
        SDL_RenderFillRect(ren, &f);
      }
      text(ren, 24, y, label, kText);
      text(ren, 150, y, "< " + value + " >", kValue);
    };
    option(kRowCrt, crt_row, 74, "CRT look", kCrts[crt].label);
    option(kRowDisplay, display_row, 93, "Display", s.fullscreen ? "Fullscreen" : "Window");
    option(kRowInput, input_row, 112, "Input", s.original_input ? "Original" : "Responsive");
    option(kRowColor, color_row, 131, "Color", s.alternative_color ? "Alternative" : "Original");
    const char* about = row == kRowCrt       ? kCrts[crt].about
                        : row == kRowDisplay ? "F11 switches in the game too."
                        : row == kRowInput   ? (s.original_input ? "As in 1990: quick presses may be lost."
                                                                 : "Every click and key press counts.")
                        : row == kRowColor   ? (s.alternative_color ? "A dark blue grid on the F2 display."
                                                                    : "As in 1990: the F2 grid in cyan.")
                                             : "";
    centred(ren, 150, about, kDim);
    // The buttons, or why the game cannot start.
    auto button = [&](int r, const SDL_FRect& f, const char* label) {
      colour(ren, row == r ? kHighlight : kButton);
      SDL_RenderFillRect(ren, &f);
      colour(ren, row == r ? kValue : kDim);
      SDL_RenderRect(ren, &f);
      text(ren, f.x + (f.w - float(std::strlen(label)) * kChar) / 2, f.y + 5, label, row == r ? kText : kDim);
    };
    if (can_start) {
      button(kRowStart, start_button, "START");
    } else {
      float y = 160;
      for (size_t at = 0; at <= s.error.size();) {  // the message, wrapped at 38 characters
        size_t end = std::min(s.error.size(), at + 38);
        if (end < s.error.size()) {
          const size_t space = s.error.rfind(' ', end);
          if (space != std::string::npos && space > at) end = space;
        }
        centred(ren, y, s.error.substr(at, end - at), kWarn);
        y += 10;
        at = end + 1;
        if (y > 170) break;
      }
    }
    button(kRowQuit, can_start ? quit_button : SDL_FRect{120, 190, 80, 18}, "QUIT");
    centred(ren, 222, "Arrows/mouse  Enter: select  Esc: quit", kDim);
    // RSR_LAUNCHER_SHOT=FILE.bmp (tests): the first frame saved, then quit.
    if (const char* shot = SDL_getenv("RSR_LAUNCHER_SHOT"); shot && *shot) {
      if (SDL_Surface* frame = SDL_RenderReadPixels(ren, nullptr)) {
        SDL_SaveBMP(frame, shot);
        SDL_DestroySurface(frame);
      }
      done = true;
    }
    SDL_RenderPresent(ren);
  }

  if (result) s.crt = kCrts[crt].preset;
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  return result;
}

}  // namespace launcher
