// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red's text buffers, kept in C++ (mem/module_state.hpp): two in its own
// BSS hunk (zeroed at load, kept while Red stays loaded), the names in its
// main BSS (cleared at every start).
#pragma once

#include "game/mem/module_state.hpp"

namespace game::red {

struct Texts {
  char mission_line[512];  // the three mission names together
  char news_front[264];    // the war news text, an outcome's text
  char commander_name[32];  // from the player's record
  char sub_name[16];        // the submarine's name (rank messages: at most 13 bytes)
  // itoa's and the status lines' buffer, and the day's news text 41 bytes
  // into it: lines built in number_text run over news_day (the original's
  // layout, a4+15685..15981).
  char numbers[297];
};

extern Texts texts_of[5];  // only Red's (index 4) is used
Texts& texts();
inline char* number_text() { return texts().numbers; }
inline char* news_day() { return texts().numbers + 41; }

inline constexpr mem::module_state::FieldAt kTextFields[] = {
    MODULE_FIELD_BE(Texts, mission_line, 512, mem::module_state::kAbsent, mem::module_state::kAbsent,
                    mem::module_state::kAbsent, mem::module_state::kAbsent, -41160),
    MODULE_FIELD_BE(Texts, news_front, 264, mem::module_state::kAbsent, mem::module_state::kAbsent,
                    mem::module_state::kAbsent, mem::module_state::kAbsent, -40648),
    MODULE_FIELD_BE(Texts, commander_name, 32, mem::module_state::kAbsent, mem::module_state::kAbsent,
                    mem::module_state::kAbsent, mem::module_state::kAbsent, 15996),
    MODULE_FIELD_BE(Texts, sub_name, 16, mem::module_state::kAbsent, mem::module_state::kAbsent,
                    mem::module_state::kAbsent, mem::module_state::kAbsent, 16028),
    MODULE_FIELD_BE(Texts, numbers, 297, mem::module_state::kAbsent, mem::module_state::kAbsent,
                    mem::module_state::kAbsent, mem::module_state::kAbsent, 15685),
};

}  // namespace game::red
