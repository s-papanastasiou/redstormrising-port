// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Not the original's: the detailed map (--detailed-map, the launch
// screen's Map: Detailed). The naval ports the briefings name are marked
// on the strategic map with a white anchor and their name, and Spitsbergen,
// the island the patrol orders are given from, is named on it in black as
// the map's own island names. Names are in the game's own font (font 1,
// bold, as the names painted into the map).
//
// The marks are drawn on the finished frame, never into the game's
// bitmaps: the game reads the map's pixels (land or sea for routes and
// movement, the fronts' colours), so its map stays the original's. A mark
// pixel is drawn only where the frame shows the clean map (bitmap 2) at
// that point, so units, the cursor and the fronts' changes are drawn over
// the marks, as if the marks were part of the map's picture.
#include "game/mem/call.hpp"
#include "game/mem/os.hpp"
#include "game/red/vars.hpp"
#include "game/screen/calls.hpp"
#include "game/ui/state.hpp"
#include "runtime/runtime.hpp"

#include <vector>

namespace game::red {

namespace {

constexpr int kWidth = 320, kHeight = 200;

// Where a port's name goes: right or left of its anchor, or above it
// (ending at it).
enum class Side { kRight, kLeft, kAbove };

struct Port {
  int16_t place;            // its point in the places table
  int8_t shift_x, shift_y;  // the anchor moved from that point (Bergen's falls on the map's NORWAY)
  const char* name;         // as the briefings spell it
  Side side;
};
constexpr Port kPorts[] = {
    {52, 0, 0, "MURMANSK", Side::kRight},
    {61, 0, 0, "ARKHANGEL'SK", Side::kAbove},
    {39, 0, 0, "NARVIK", Side::kLeft},
    {35, 0, 0, "TRONDHEIM", Side::kLeft},
    {28, -2, -2, "BERGEN", Side::kLeft},
    {16, 0, 0, "THORSHAVN", Side::kRight},  // the Faeroes
};

// An island's name, its lines centred on (x, y): Spitsbergen, the island
// in the top right, which has no port point.
struct IslandName {
  int16_t x, y;
  const char* lines[2];
};
constexpr IslandName kIslands[] = {{265, 36, {"SPITS", "BERGEN"}}};

// The anchor, 5 x 6, its shank on the point (x - 2 .. x + 2, y - 3 .. y + 2).
constexpr const char* kAnchor[] = {"..#..", ".###.", "..#..", "..#..", "#.#.#", ".###."};

struct Pixel {
  int16_t x, y;
  uint32_t colour;
};

// A text's width in the font (the advances), and its pixels at (x, y)
// (the top of the font), drawn bold: each pixel and the one right of it.
int text_width(const amiga::Drawing::Font& f, const char* text) {
  int w = 0;
  for (const char* c = text; *c; ++c) {
    const int g = f.glyph(uint8_t(*c));
    w += f.space.empty() ? f.xsize : f.space[size_t(g)];
  }
  return w;
}
void text_pixels(const amiga::Drawing::Font& f, int x, int y, const char* text, uint32_t colour,
                 std::vector<Pixel>& out) {
  for (const char* c = text; *c; ++c) {
    const int g = f.glyph(uint8_t(*c));
    const int kern = f.kern.empty() ? 0 : f.kern[size_t(g)];
    for (int row = 0; row < f.ysize; ++row)
      for (int col = 0; col < f.bits[size_t(g)]; ++col)
        if (f.pixel(row, f.bit0[size_t(g)] + col)) {
          out.push_back({int16_t(x + kern + col), int16_t(y + row), colour});
          out.push_back({int16_t(x + kern + col + 1), int16_t(y + row), colour});
        }
    x += f.space.empty() ? f.xsize : f.space[size_t(g)];
  }
}

// The marks' pixels, made once from the font (the places do not move).
const std::vector<Pixel>& mark_pixels(uint32_t font) {
  static uint32_t made_for = 0;
  static std::vector<Pixel> pixels;
  if (made_for == font) return pixels;
  const amiga::Drawing::Font& f = os::text_font(font);
  pixels.clear();
  for (const Port& p : kPorts) {
    const int x = places[p.place].pos.x + p.shift_x, y = places[p.place].pos.y + p.shift_y;
    for (int row = 0; row < 6; ++row)
      for (int col = 0; col < 5; ++col)
        if (kAnchor[row][col] == '#') pixels.push_back({int16_t(x - 2 + col), int16_t(y - 3 + row), kMarkColour});
    const int w = text_width(f, p.name);
    switch (p.side) {
      case Side::kRight: text_pixels(f, x + 6, y - 2, p.name, kMarkColour, pixels); break;
      case Side::kLeft: text_pixels(f, x - 6 - w, y - 2, p.name, kMarkColour, pixels); break;
      case Side::kAbove: text_pixels(f, x + 1 - w, y - 8, p.name, kMarkColour, pixels); break;
    }
  }
  for (const IslandName& island : kIslands) {
    constexpr int kGap = 2;  // between the lines
    const int top = island.y - (2 * f.ysize + kGap) / 2;
    for (int line = 0; line < 2; ++line) {
      const int w = text_width(f, island.lines[line]) + 1;  // with the bold pixel
      text_pixels(f, island.x - w / 2, top + line * (f.ysize + kGap), island.lines[line], kIslandNameColour, pixels);
    }
  }
  std::erase_if(pixels, [](const Pixel& q) { return q.x < 0 || q.y < 0 || q.x >= kWidth || q.y >= kHeight; });
  made_for = font;
  return pixels;
}

// The frame overlay (os::set_frame_overlay): while Red's strategic map is
// shown, each mark pixel where the shown bitmap has the clean map's pen.
void draw_marks(uint32_t* frame) {
  if (!mem::detailed_map() || runtime::module_index() != runtime::kRedModule || !map_active()) return;
  const uint32_t font = ui::state().fonts[kMarkFont];
  if (!font) return;
  const auto& shown = screen::bitmap_at(screen::shown_bitmap());
  const auto& clean = screen::bitmap_at(kMapBitmap);
  for (const Pixel& p : mark_pixels(font))
    if (os::bitmap_pixel(shown, p.x, p.y) == os::bitmap_pixel(clean, p.x, p.y))
      frame[size_t(p.y) * kWidth + size_t(p.x)] = p.colour;
}

const bool kRegistered = (os::set_frame_overlay(draw_marks), true);

}  // namespace

}  // namespace game::red
