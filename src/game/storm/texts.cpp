// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Storm's text buffers (texts.hpp).
#include "game/storm/texts.hpp"

#include "runtime/runtime.hpp"

namespace game::storm {

Texts texts_of[5];
MODULE_STATE("storm texts", texts_of, kTextFields);

Texts& texts() { return texts_of[runtime::module_index()]; }

}  // namespace game::storm
