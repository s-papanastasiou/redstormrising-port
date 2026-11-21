// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Input: waiting in RedStart and Red, with the frame ticks running (the
// menu input, the map's animation): until the keys and buttons are
// released, until a key, or for some seconds.
#include "game/audio/calls.hpp"
#include "game/input/calls.hpp"
#include "game/input/vars.hpp"
#include "game/mem/register.hpp"
#include "game/screen/calls.hpp"
#include "game/ui/state.hpp"

namespace game::input {

namespace {
void tick() {
  ui::state().menu_flags &= ~kMenuKeyRead;
  frame_tick();
}
}  // namespace

// $28c12: wait until fire and the select button are released.
void wait_released() {
  flush_keys();
  do tick();
  while ((ui::state().menu_flags & kMenuFire) || ui::state().select_button);
  flush_keys();
}
ORIGINAL(wait_released, 0, 0, 0, 0x26d16, 0x28c12);

// $28be6: wait for a key (character 169), fire or the select button.
void wait_key() {
  wait_released();
  for (;;) {
    tick();
    screen::wait_tof();
    if (ui::state().quit_key || (ui::state().menu_flags & kMenuFire) || ui::state().select_button) break;
  }
  wait_released();
}
ORIGINAL(wait_key, 0, 0, 0, 0x26cea, 0x28be6);

// $28c34: wait n seconds (not at all while quit_key is set; the
// bitmaps stop flipping); fire ends the wait early and gives 1.
int16_t wait_seconds(int16_t n) {
  const uint32_t deadline = uint32_t(int32_t(int16_t(n * kFramesPerSecond))) + audio::frames();
  if (ui::state().quit_key) return 0;
  wait_released();
  ui::state().double_buffer = 0;
  while (audio::frames() < deadline) {
    screen::wait_tof();
    tick();
    if (ui::state().menu_flags & kMenuFire) {
      wait_released();
      return 1;
    }
    if (ui::state().quit_key) break;
  }
  return 0;
}
ORIGINAL(wait_seconds, 0, 0, 0, 0x26d38, 0x28c34);

}  // namespace game::input
