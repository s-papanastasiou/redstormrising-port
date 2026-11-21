// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Storm's text buffers, kept in C++ (mem/module_state.hpp).
#pragma once

#include "game/mem/module_state.hpp"

namespace game::storm {

struct Texts {
  char text_buffer[40];  // a character or a status message being built
  char last_status[40];  // the status message last remembered
  char hit_message[40];  // a hit's message being built
  char typed_digits[4];  // the number typed (with its terminator)
};

extern Texts texts_of[5];  // only Storm's (index 0) is used
Texts& texts();

inline constexpr mem::module_state::FieldAt kTextFields[] = {
    MODULE_FIELD_BE(Texts, text_buffer, 40, -8794, mem::module_state::kAbsent, mem::module_state::kAbsent,
                    mem::module_state::kAbsent, mem::module_state::kAbsent),
    MODULE_FIELD_BE(Texts, last_status, 40, -8714, mem::module_state::kAbsent, mem::module_state::kAbsent,
                    mem::module_state::kAbsent, mem::module_state::kAbsent),
    MODULE_FIELD_BE(Texts, hit_message, 40, -8754, mem::module_state::kAbsent, mem::module_state::kAbsent,
                    mem::module_state::kAbsent, mem::module_state::kAbsent),
    MODULE_FIELD_BE(Texts, typed_digits, 4, -7106, mem::module_state::kAbsent, mem::module_state::kAbsent,
                    mem::module_state::kAbsent, mem::module_state::kAbsent),
};

}  // namespace game::storm
