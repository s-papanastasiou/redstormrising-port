// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The animation player's state (anim_state.hpp).
#include "game/screen/anim_state.hpp"

#include "runtime/runtime.hpp"

namespace game::screen {

Anim anims[5];
MODULE_STATE("anim", anims, kAnimFields);

Anim& anim() { return anims[runtime::module_index()]; }

}  // namespace game::screen
