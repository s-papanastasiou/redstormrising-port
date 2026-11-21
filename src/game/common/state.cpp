// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The C runtime's and the hand-over's state (state.hpp): a copy per module.
#include "game/common/state.hpp"

#include "runtime/runtime.hpp"

namespace game::common {

State states[5];
MODULE_STATE("common", states, kFields);

State& state() { return states[runtime::module_index()]; }

}  // namespace game::common
