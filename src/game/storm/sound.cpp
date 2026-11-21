// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Storm: the battle's use of the sound player.
#include "game/audio/calls.hpp"
#include "game/storm/vars.hpp"

namespace game::storm {

// Storm $21474: once per battle frame: in the silent sub-mode the engine
// noise stops and a pending effect plays; otherwise the engine noise
// follows the speed and the quiet-mode flag.
void engine_update() {
  if (battle.sub_mode == kSilentSubMode) {
    audio::engine_off();
    if (sight.pending_effect != 0) audio::play_effect(sight.pending_effect);
    return;
  }
  audio::set_engine(ships[0].speed);  // the player's submarine: its speed, cavitating
  audio::sound_mode(int16_t(ships[0].flags & 1));
  audio::engine_on();
}
ORIGINAL_IN("storm", 0x21474, engine_update);

}  // namespace game::storm
