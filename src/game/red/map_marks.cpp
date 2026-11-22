// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Not the original's: the detailed map (--detailed-map, the launch
// screen's Map: Detailed). The naval ports the briefings name are marked
// on the strategic map with a white anchor and their name in the game's
// own font (font 1, bold, as the names painted into the map).
//
// The marks are drawn on the finished frame, never into the game's
// bitmaps: the game reads the map's pixels (land or sea for routes and
// movement, the fronts' colours), so its map stays the original's. A mark
// is drawn piece by piece (the anchor, each letter), and a piece only
// where the frame shows the clean map (bitmap 2) under all its pixels: a
// unit, the cursor or a front's change on it hides that piece whole, so
// they stay on top of the marks, clean, as if the marks were part of the
// map's picture.
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

struct Mark {
  int16_t place;          // its point in the places table
  int8_t shift_x, shift_y;  // the anchor moved from that point (Bergen's falls on the map's NORWAY)
  const char* name;       // as the briefings spell it
  Side side;
};
constexpr Mark kMarks[] = {
    {52, 0, 0, "MURMANSK", Side::kRight},
    {61, 0, 0, "ARKHANGEL'SK", Side::kAbove},
    {39, 0, 0, "NARVIK", Side::kLeft},
    {35, 0, 0, "TRONDHEIM", Side::kLeft},
    {28, -2, -2, "BERGEN", Side::kLeft},
};

// The anchor, 5 x 6, its shank on the point (x - 2 .. x + 2, y - 3 .. y + 2).
constexpr const char* kAnchor[] = {"..#..", ".###.", "..#..", "..#..", "#.#.#", ".###."};

struct Pixel {
  int16_t x, y;
};
// A piece of a mark (the anchor, a letter): drawn whole or not at all.
struct Piece {
  std::vector<Pixel> pixels;
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
void text_pieces(const amiga::Drawing::Font& f, int x, int y, const char* text, std::vector<Piece>& out) {
  for (const char* c = text; *c; ++c) {
    const int g = f.glyph(uint8_t(*c));
    const int kern = f.kern.empty() ? 0 : f.kern[size_t(g)];
    Piece letter;
    for (int row = 0; row < f.ysize; ++row)
      for (int col = 0; col < f.bits[size_t(g)]; ++col)
        if (f.pixel(row, f.bit0[size_t(g)] + col)) {
          letter.pixels.push_back({int16_t(x + kern + col), int16_t(y + row)});
          letter.pixels.push_back({int16_t(x + kern + col + 1), int16_t(y + row)});
        }
    if (!letter.pixels.empty()) out.push_back(std::move(letter));
    x += f.space.empty() ? f.xsize : f.space[size_t(g)];
  }
}

// The marks' pieces, made once from the font (the places do not move).
const std::vector<Piece>& mark_pieces(uint32_t font) {
  static uint32_t made_for = 0;
  static std::vector<Piece> pieces;
  if (made_for == font) return pieces;
  const amiga::Drawing::Font& f = os::text_font(font);
  pieces.clear();
  for (const Mark& m : kMarks) {
    const int x = places[m.place].pos.x + m.shift_x, y = places[m.place].pos.y + m.shift_y;
    Piece anchor;
    for (int row = 0; row < 6; ++row)
      for (int col = 0; col < 5; ++col)
        if (kAnchor[row][col] == '#') anchor.pixels.push_back({int16_t(x - 2 + col), int16_t(y - 3 + row)});
    pieces.push_back(std::move(anchor));
    const int w = text_width(f, m.name);
    switch (m.side) {
      case Side::kRight: text_pieces(f, x + 6, y - 2, m.name, pieces); break;
      case Side::kLeft: text_pieces(f, x - 6 - w, y - 2, m.name, pieces); break;
      case Side::kAbove: text_pieces(f, x + 1 - w, y - 8, m.name, pieces); break;
    }
  }
  for (Piece& p : pieces)
    std::erase_if(p.pixels, [](const Pixel& q) { return q.x < 0 || q.y < 0 || q.x >= kWidth || q.y >= kHeight; });
  made_for = font;
  return pieces;
}

// The frame overlay (os::set_frame_overlay): while Red's strategic map is
// shown, each piece whose pixels all show the clean map's pens.
void draw_marks(uint32_t* frame) {
  if (!mem::detailed_map() || runtime::module_index() != runtime::kRedModule || !map_active()) return;
  const uint32_t font = ui::state().fonts[kMarkFont];
  if (!font) return;
  const auto& shown = screen::bitmap_at(screen::shown_bitmap());
  const auto& clean = screen::bitmap_at(kMapBitmap);
  for (const Piece& piece : mark_pieces(font)) {
    bool clear = true;
    for (const Pixel& p : piece.pixels)
      if (os::bitmap_pixel(shown, p.x, p.y) != os::bitmap_pixel(clean, p.x, p.y)) {
        clear = false;
        break;
      }
    if (!clear) continue;
    for (const Pixel& p : piece.pixels) frame[size_t(p.y) * kWidth + size_t(p.x)] = kMarkColour;
  }
}

const bool kRegistered = (os::set_frame_overlay(draw_marks), true);

}  // namespace

}  // namespace game::red
