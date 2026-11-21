// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Input: the menu cursor of RedStart and Red, run every frame by their
// frame ticks: a key per frame, the direction and fire bits (from the
// controller, or by steering with the mouse), one cursor step within its
// bounds.
#include "game/input/calls.hpp"
#include "game/input/vars.hpp"
#include "game/mem/register.hpp"
#include "game/ui/state.hpp"

namespace game::input {

namespace {
// |a - b| as the original computes it (16-bit, by the signed comparison).
int16_t distance(int16_t a, int16_t b) { return a < b ? int16_t(b - a) : int16_t(a - b); }

// Steering with the mouse while a button is down: steps towards the
// pointer from the reference point, in each axis in proportion to the
// distance (both distances halved until at most kMaxSteer).
int32_t steer(int32_t bits) {
  const ui::Point& mouse = ui::state().mouse_pos;
  const int16_t ref_x = *steer_ref_x, ref_y = *steer_ref_y;
  ui::Point& last = ui::state().last_mouse;
  if (mouse.x != last.x || mouse.y != last.y || ui::state().steer_step == 0) {
    last = mouse;
    ui::state().steer_step = 0;
    ui::state().steer_dx = distance(mouse.x, ref_x);
    ui::state().steer_dy = distance(mouse.y, ref_y);
    while (ui::state().steer_dx > kMaxSteer || ui::state().steer_dy > kMaxSteer) {
      ui::state().steer_dx = int16_t(ui::state().steer_dx >> 1);
      ui::state().steer_dy = int16_t(ui::state().steer_dy >> 1);
    }
  }
  const int16_t dx = ui::state().steer_dx, dy = ui::state().steer_dy;
  if (ui::state().steer_step <= dx) {
    if (mouse.x > ref_x) bits |= kRight;
    if (mouse.x < ref_x) bits |= kLeft;
  }
  if (ui::state().steer_step <= dy) {
    if (mouse.y > ref_y) bits |= kDown;
    if (mouse.y < ref_y) bits |= kUp;
  }
  ui::state().steer_step += 1;
  if (ui::state().steer_step > dx && ui::state().steer_step > dy) ui::state().steer_step = 0;
  return bits;
}

void request() {
  if (!ui::state().no_help_request) *menu_request = kRequestKey;
}
}  // namespace

// $26a84: read a key with getkey, unless one was read this frame.
void menu_key() {
  if (ui::state().menu_flags & kMenuKeyRead) return;
  ui::state().menu_key_code = getkey();
  ui::state().menu_flags = (ui::state().menu_flags & ~kMenuKeyRead) | (ui::state().menu_key_code ? kMenuKeyRead : 0);
}
ORIGINAL(menu_key, 0, 0, 0, 0x26a84, 0x28980);

// $26abe: the frame's direction and fire bits (menu_dirs, kMenuFire):
// from the controller, or with mouse steering on and a button down from
// the mouse; then the key read by menu_key (Return/Space fire or request,
// Alt-P requests, Alt-V toggles menu_toggle). Returns 0.
// With steering on and the menu button down, the original starts from its
// caller's d7; here from 0.
int32_t menu_controls() {
  int32_t bits;
  if (!ui::state().mouse_steering) bits = controls() & 0xff;
  else if (ui::state().select_button == 0) bits = controls();
  else bits = steer(ui::state().select_button == 1 ? kFire : 0);
  if (ui::state().menu_flags & kMenuKeyRead) {
    switch (uint16_t(ui::state().menu_key_code)) {
      case kReturn:
      case kSpace:
        if (!ui::state().space_requests) bits |= kFire;
        else request();
        break;
      case kKeyAltP: request(); break;
      case kKeyAltV: ui::state().menu_toggle = ui::state().menu_toggle ? 0 : 1; break;
      default: break;  // 169: the original's test for it can never pass
    }
    ui::state().menu_key_code = 0;
  }
  ui::state().menu_dirs = int16_t(bits);
  ui::state().menu_flags = (ui::state().menu_flags & ~kMenuFire) | ((bits & kFire) ? kMenuFire : 0);
  return 0;
}
ORIGINAL(menu_controls, 0, 0, 0, 0x26abe, 0x289ba);

// $26c7e: once per frame, one cursor step by the direction bits, within
// the bounds.
void menu_move() {
  if (!(ui::state().menu_flags & kMenuMoved)) {
    const uint8_t dirs = uint8_t(ui::state().menu_dirs);
    ui::Point& cur = ui::state().menu_cursor;
    const Bounds b = menu_bounds();
    if ((dirs & kDown) && cur.y < b.bottom) cur.y += 1;
    if ((dirs & kUp) && cur.y > b.top) cur.y -= 1;
    if ((dirs & kLeft) && cur.x > b.left) cur.x -= 1;
    if ((dirs & kRight) && cur.x < b.right) cur.x += 1;
  }
  ui::state().menu_flags |= kMenuMoved;
}
ORIGINAL(menu_move, 0, 0, 0, 0x26c7e, 0x28b7a);

}  // namespace game::input
