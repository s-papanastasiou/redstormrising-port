// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The sound code's state (state.hpp): a copy per module.
#include "game/audio/state.hpp"

#include "runtime/runtime.hpp"

namespace game::audio {

State states[5];
MODULE_STATE("audio", states, kFields);

State& state() { return states[runtime::module_index()]; }

}  // namespace game::audio
