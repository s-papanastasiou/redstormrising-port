// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Text styles kept in C++. The
// game names a style by its address (stored in the menus' pointers, passed
// to the drawing code); that address stays the style's handle, and
// style_at() finds the C++ object for the moved ones (else the style in
// memory). StyleRef replaces a stored Ptr<TextStyle>, StyleVar a
// declaration Var<TextStyle>, with the same syntax.
#pragma once

#include "game/mem/module_state.hpp"
#include "game/screen/vars.hpp"

namespace game::screen {

TextStyle& style_at(uint32_t handle);

struct StyleRef {
  be32 addr;
  TextStyle* operator->() const { return &style_at(addr); }
  TextStyle& operator*() const { return style_at(addr); }
  StyleRef& operator=(uint32_t a) {
    addr = a;
    return *this;
  }
};
static_assert(sizeof(StyleRef) == 4);

struct StyleVar {
  int32_t off;  // its a4 offset
  uint32_t addr() const { return mem::a4() + uint32_t(off); }
  TextStyle& operator*() const { return style_at(addr()); }
  TextStyle* operator->() const { return &style_at(addr()); }
};

// The moved styles of each module (a slot each; storm, redstorm, title,
// redstart, red).
inline constexpr int kStyleSlots = 3;
struct Styles {
  TextStyle slots[kStyleSlots];
};
inline constexpr int32_t A = mem::module_state::kAbsent;
inline constexpr int32_t kStyleAt[5][kStyleSlots] = {
    {-32764, A, A},          // Storm: text_style
    {A, A, A},
    {A, A, A},
    {-32768, -15592, -15648},  // RedStart: page_style, row_style, window_text_style
    {28, 52, A},             // Red: page_style, symbol_style
};
inline constexpr mem::module_state::FieldAt kStyleFields[] = {
    MODULE_FIELD_BE(Styles, slots[0], 24, kStyleAt[0][0], kStyleAt[1][0], kStyleAt[2][0], kStyleAt[3][0], kStyleAt[4][0]),
    MODULE_FIELD_BE(Styles, slots[1], 24, kStyleAt[0][1], kStyleAt[1][1], kStyleAt[2][1], kStyleAt[3][1], kStyleAt[4][1]),
    MODULE_FIELD_BE(Styles, slots[2], 24, kStyleAt[0][2], kStyleAt[1][2], kStyleAt[2][2], kStyleAt[3][2], kStyleAt[4][2]),
};

}  // namespace game::screen
