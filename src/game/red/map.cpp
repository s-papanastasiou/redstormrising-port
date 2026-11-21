// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red (the campaign module): the player's submarine on the strategic map.
#include "game/red/calls.hpp"
#include "game/red/vars.hpp"
#include "game/screen/calls.hpp"
#include "game/ui/state.hpp"

#include <algorithm>
#include <iterator>

namespace game::red {

// Red $227d4: move the player's submarine towards the map cursor. Runs
// every map update; acts on every 8th. When the submarine's movement
// accumulator carries (unit_step), the cursor position is accepted as the
// new position if the map is navigable there, otherwise the cursor is put
// back on the submarine. Then the enemy's detection of the player and the
// scroll input are updated.
void scroll_input() {
  MapUnit& sub = map_units[kPlayerSub];
  sub.status |= kMoved | kArrived;  // in the view
  if ((map_ticks() & 7) != 7) return;
  scroll_speed();
  if (unit_step(kPlayerSub)) {
    ui::state().menu_flags &= ~kCursorMoved;
    screen::draw_to(kMapBitmap);
    const int16_t colour = map_pixel(map_cursor().x, map_cursor().y);
    screen::draw_to(map_draw_bitmap());
    if (std::find(std::begin(kNavigable), std::end(kNavigable), colour) != std::end(kNavigable))
      sub.pos = position(map_cursor());
    else
      map_cursor() = ui_point(sub.pos);
  }
  detect(kPlayerSub);
  sub.status &= uint8_t(~kSeen);
  input_bits();
}
ORIGINAL_IN("red", 0x227d4, scroll_input);

}  // namespace game::red
