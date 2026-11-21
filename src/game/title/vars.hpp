// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Title (the opening: pictures and the title sequence): variables used by
// the game code. Offsets are a4-relative in Title's small-data area (a4 =
// $2d714).
#pragma once

#include "game/mem/constants.hpp"
#include "game/mem/globals.hpp"

namespace game::title {

using namespace game::mem;

inline constexpr Const<be16> title_palette{-30576};  // [screen::kPaletteWords]
inline constexpr int16_t kFirstOpening = 1, kSecondOpening = 2, kSequenceEffect = 3, kAfterEffect = 0;  // sound effects started with and after the sequence

}  // namespace game::title
