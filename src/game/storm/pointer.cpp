// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Storm: the pointer moved by the game (aiming, steering): one input
// event per move (game::platform::write_input_event).
#include "game/common/strings.hpp"
#include "game/common/vars.hpp"
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/platform/platform.hpp"
#include "game/screen/calls.hpp"
#include "game/screen/vars.hpp"
#include "game/storm/calls.hpp"
#include "game/storm/vars.hpp"
#include "game/ui/state.hpp"

namespace game::storm {

// $32534 / $3269a: input.device opened at the start of a battle (an
// event, a port, a request) and closed at its end: with the host's input
// service there is nothing to open.
void open_input() {}
ORIGINAL(open_input, 0x32534, 0, 0, 0, 0);
void close_input() {}
ORIGINAL(close_input, 0x3269a, 0, 0, 0, 0);

// $325a8: the pointer to (x, y) on the battle screen (relative: moved by
// x, y), unless another screen is in front. PAL screens sit 10 lines
// lower, the event's coordinates are in half pixels.
void move_pointer(int16_t x, int16_t y, int16_t relative) {
  const uint32_t lock = os::lock_ibase(0);
  const bool elsewhere =
      os::active_screen() != (ui::state().screen).addr;
  os::unlock_ibase(lock);
  if (elsewhere) return;
  if (relative) {
    platform::write_input_event(kClassRawMouse, kRelativeMouse, int16_t(x * 2), int16_t(y * 2));
  } else {
    if (os::pal_display()) y = int16_t(y + kPalPointerLines);
    platform::write_input_event(kClassPointerPos, 0, int16_t(x * 2), int16_t(y * 2));
  }
}
ORIGINAL(move_pointer, 0x325a8, 0, 0, 0, 0);

// $2afcc: the identification display's update: nothing (the list is drawn once).
void identify_update() {}
ORIGINAL(identify_update, 0x2afcc, 0, 0, 0, 0);

}  // namespace game::storm
