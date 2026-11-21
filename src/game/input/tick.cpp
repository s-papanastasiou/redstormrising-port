// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Input: the frame tick of RedStart and Red, called by every waiting
// loop: the bitmaps' flip (while the map is animated), a sound every 64
// ticks, then the menu input.
#include "game/audio/calls.hpp"
#include "game/input/calls.hpp"
#include "game/input/vars.hpp"
#include "game/mem/hardware.hpp"
#include "game/mem/register.hpp"
#include "game/screen/calls.hpp"
#include "game/ui/state.hpp"

namespace game::input {

namespace {
// Until the timer has moved on by two ticks since the last wait.
void wait_two_ticks() {
  hw::wait_until([] { return audio::frames() - ui::state().last_tick >= kTicksPerWait; });
  ui::state().last_tick = audio::frames();
}

void tick_tail() {
  ui::state().tick_frames += 1;
  if ((*tick_sound_a || *tick_sound_b) && !(ui::state().tick_frames & (kTickSoundEvery - 1)) && ui::state().space_requests)
    audio::play_effect(kTickSound);
  if (ui::state().mouse_steering) ui::state().menu_flags &= ~kMenuKeyRead;
  menu_key();
  menu_controls();
  menu_move();
  if (ui::state().quit_key) *menu_request = kRequestQuit;
}
}  // namespace

// $288e0 (RedStart $269e4): every third tick (while mouse_steering) the
// bitmap drawn into is shown and the other one drawn into (with
// double_buffer), then the timer is waited for; then the tail.
void frame_tick() {
  if (ui::state().mouse_steering && (ui::state().flip_countdown -= 1) <= 0) {
    if (ui::state().double_buffer) {
      screen::show_bitmap(ui::state().flip_bitmap);
      ui::state().flip_bitmap = int16_t(ui::state().flip_bitmap ^ 1);
      screen::draw_to(ui::state().flip_bitmap);
    }
    ui::state().frame_ticked = 1;
    ui::state().flip_countdown = kFlipEvery;
    ui::state().flips += 1;
    wait_two_ticks();
  }
  tick_tail();
}
ORIGINAL(frame_tick, 0, 0, 0, 0x269e4, 0x288e0);

// (Red $2891a, frame_tick_wait: not reached by the game.)

}  // namespace game::input
