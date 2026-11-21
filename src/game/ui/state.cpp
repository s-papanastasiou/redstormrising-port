// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The shared UI state (state.hpp): a copy per module.
#include "game/ui/state.hpp"

#include "runtime/runtime.hpp"

namespace game::ui {

State states[5];
MODULE_STATE("ui", states, kFields);

State& state() { return states[runtime::module_index()]; }

}  // namespace game::ui
