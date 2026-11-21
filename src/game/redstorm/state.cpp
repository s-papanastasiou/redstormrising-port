// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// RedStorm's state (state.hpp).
#include "game/redstorm/state.hpp"

#include "runtime/runtime.hpp"

namespace game::redstorm {

State states[5];
MODULE_STATE("redstorm", states, kFields);

State& state() { return states[runtime::module_index()]; }

}  // namespace game::redstorm
