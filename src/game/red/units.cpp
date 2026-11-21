// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red (the campaign module): units on the strategic map.
#include "game/audio/calls.hpp"
#include "game/red/calls.hpp"
#include "game/red/vars.hpp"
#include "game/screen/calls.hpp"
#include "game/ui/state.hpp"

namespace game::red {

// Red $2c9fa: advance a unit's movement accumulator by its speed. Returns
// 1 when the accumulator passes 255 (the unit moves one map step; the
// carry is dropped), else 0.
int16_t unit_step(int16_t unit) {
  UnitRecord& r = unit_records[unit];
  const uint16_t progress = uint16_t(r.progress + r.step);
  if (progress > 0xFF) {
    r.progress = uint16_t(progress & 0xFF);
    return 1;
  }
  r.progress = progress;
  return 0;
}
ORIGINAL_IN("red", 0x2c9fa, unit_step);

// Red $2884a: play_effect(0), the empty effect.
void effect_none() { audio::play_effect(kEffectNone); }
ORIGINAL_IN("red", 0x2884a, effect_none);

// $23ec4: the map screen's mode: leaving a text page (if one is up) the
// map's animation and controls are on again; the module's palette and the
// text page style are current.
void text_screen() {
  if (ui::state().page_shown) {
    effect_none();
    ui::state().page_shown = 0;
    map_controls() = 1;
    map_active() = 1;
    ui::state().unread_page_flag = 0;
  }
  screen::default_palette();
  ui::state().page_text_style = page_style.addr();
}
ORIGINAL_IN("red", 0x23ec4, text_screen);

}  // namespace game::red
