// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Text styles kept in C++ (styles.hpp).
#include "game/screen/styles.hpp"

#include "runtime/runtime.hpp"

namespace game::screen {

namespace {
Styles g_styles[5];
MODULE_STATE("styles", g_styles, kStyleFields);
}  // namespace

TextStyle& style_at(uint32_t handle) {
  const int m = runtime::module_index();
  const int32_t off = int32_t(handle - mem::a4());
  for (int s = 0; s < kStyleSlots; ++s)
    if (kStyleAt[m][s] != mem::module_state::kAbsent && kStyleAt[m][s] == off) return g_styles[m].slots[s];
  return *ptr_to<TextStyle>(handle);
}

}  // namespace game::screen
