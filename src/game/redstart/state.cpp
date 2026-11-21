// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// RedStart's state (state.hpp).
#include "game/redstart/state.hpp"

#include "runtime/runtime.hpp"

namespace game::redstart {

State states[5];
MODULE_STATE("redstart", states, kFields);

State& state() { return states[runtime::module_index()]; }

}  // namespace game::redstart
