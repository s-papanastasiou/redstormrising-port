// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red's text buffers (texts.hpp).
#include "game/red/texts_state.hpp"

#include "runtime/runtime.hpp"

namespace game::red {

Texts texts_of[5];
MODULE_STATE("red texts", texts_of, kTextFields);

Texts& texts() { return texts_of[runtime::module_index()]; }

}  // namespace game::red
