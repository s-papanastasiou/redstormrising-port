// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Input: keys and mouse buttons (and the joystick port, which nothing
// connects), read by every module (the screen/input library).
#include "game/input/calls.hpp"
#include "game/input/vars.hpp"
#include "game/mem/hardware.hpp"
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/platform/platform.hpp"
#include "game/ui/state.hpp"

namespace game::input {

namespace {
// The key table for a qualifier byte (none: 0; Shift: 1, 2; Alt: 16, 32;
// other combinations are ignored).
const ConstLib<uint8_t>* table_for(uint8_t qualifier) {
  switch (qualifier) {
    case 0: return &keys_plain;
    case 1: case 2: return &keys_shift;
    case 16: case 32: return &keys_alt;
    default: return nullptr;
  }
}
}  // namespace

// $315da: read the window's pending messages: mouse buttons set
// select_button, key presses are translated to characters. Returns the
// character of the last key pressed (0: none), also kept in last_key.
int16_t getkey() {
  int16_t key = 0;
  platform::InputEvent event;
  while (platform::next_input(event)) {
    const uint32_t cls = event.cls;
    const uint16_t code = event.code, qualifier = event.qualifier;
    ui::state().button_event = 0;
    if (cls == IDCMP_MOUSEBUTTONS) {
      ui::state().button_event = 1;
      if (code == SELECTDOWN) ui::state().select_button = 1;
      else if (code == MENUDOWN) ui::state().select_button = 2;
      else if (code == SELECTUP || code == MENUUP) ui::state().select_button = 0;
    } else if (cls == IDCMP_RAWKEY && !(code & IECODE_UP_PREFIX)) {
      if (const ConstLib<uint8_t>* table = table_for(uint8_t(qualifier))) key = (*table)[code];
    }
  }
  ui::state().last_key = key;
  return key;
}
ORIGINAL(getkey, 0x315da, 0x216ea, 0x23422, 0x2371a, 0x27682);

// $316f8: forget pending keys and the select button.
void flush_keys() {
  while (getkey()) {}
  ui::state().select_button = 0;
}
ORIGINAL(flush_keys, 0x316f8, 0x21808, 0x23540, 0x23838, 0x277a0);
ORIGINAL(flush_keys, 0x31706, 0x21816, 0x2354e, 0x23846, 0x277ae);  // wrappers
ORIGINAL(flush_keys, 0x3295c, 0, 0, 0x25124, 0x290cc);

// $3170a: the joystick in port 2 as direction bits, plus kFire for its
// button or the select button. A direction ends a mouse-button event.
int32_t read_stick() {
  const platform::Joystick stick = platform::joystick();
  const bool button = stick.fire;
  // The original decoded JOY1DAT through a table of the eight directions:
  // opposite directions pressed together give none.
  int32_t bits = 0;
  if (!(stick.right && stick.left) && !(stick.up && stick.down))
    bits = (stick.right ? kRight : 0) | (stick.left ? kLeft : 0) | (stick.down ? kDown : 0) | (stick.up ? kUp : 0);
  if (bits) ui::state().button_event = 0;
  if (button || ui::state().select_button != 0) bits |= kFire;
  return bits;
}
ORIGINAL(read_stick, 0x3170a, 0x2181a, 0x23552, 0x2384a, 0x277b2);

// $314ea: the character of a key held down now (0: none, or another
// screen is in front), from the keyboard matrix; also kept in last_key.
int16_t key_down() {
  const uint32_t lock = os::lock_ibase(0);
  const bool ours = os::active_screen() == ui::state().screen.addr;
  os::unlock_ibase(lock);
  if (!ours) return 0;
  const uint8_t* key_matrix = ui::state().key_matrix;
  platform::read_key_matrix(ui::state().key_matrix, kMatrixKeyBytes + 1);
  const uint8_t qualifiers = key_matrix[kMatrixQualifiers];
  const ConstLib<uint8_t>* table = &keys_plain;
  if (qualifiers & kMatrixAlt) table = &keys_alt;
  if (qualifiers & kMatrixShift) table = &keys_shift;
  uint8_t ch = 0;
  for (int byte = 0; byte < kMatrixKeyBytes && !ch; ++byte)
    for (int bit = 0; bit < 8 && !ch; ++bit)
      if (key_matrix[byte] & (1 << bit)) ch = (*table)[byte * 8 + bit];
  ui::state().last_key = ch;
  return ch;
}
ORIGINAL(key_down, 0x314ea, 0x215fa, 0x23332, 0x2362a, 0x27592);

// $32c38: the "controller": read_stick's bits plus the keys held down
// (Return and Space fire, the cursor keys steer, 132-135 steer and fire,
// 169 fires and sets quit_key) and the select button.
int32_t controls() {
  int32_t bits = read_stick();
  switch (uint16_t(key_down())) {
    case kReturn: if (!*no_return_fire) bits |= kFire; break;
    case kSpace: bits |= kFire; break;
    case kCursorUp: bits |= kUp; break;
    case kCursorLeft: bits |= kLeft; break;
    case kCursorRight: bits |= kRight; break;
    case kCursorDown: bits |= kDown; break;
    case kCursorDown + 1: bits |= kUp | kFire; break;
    case kCursorDown + 2: bits |= kLeft | kFire; break;
    case kCursorDown + 3: bits |= kRight | kFire; break;
    case kCursorDown + 4: bits |= kDown | kFire; break;
    case kKeyAltQ: ui::state().quit_key = 1; bits |= kFire; break;
    default: break;
  }
  if (ui::state().select_button == 1) bits |= kFire;
  return bits;
}
ORIGINAL(controls, 0x32c38, 0, 0, 0x25400, 0x293a8);
ORIGINAL(controls, 0x2d3c0, 0, 0, 0x2550e, 0x294b6);  // wrappers

}  // namespace game::input
