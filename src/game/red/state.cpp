// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red's native state (state.hpp): its registration.
#include "game/red/state.hpp"

#include "game/mem/state.hpp"

namespace game::red {

GAME_STATE_FIELDS(4, "red", state, kMovedRanges, kScriptFields);

namespace {
void load_data() {
  using mem::state::loaded_image;
  mem::image::read(loaded_image(kFrontsImage), data.fronts);
  mem::image::read(loaded_image(kPlacesImage), data.places);
  mem::image::read(loaded_image(kKindBeforeImage), data.kind_before);
  mem::image::read(loaded_image(134), data.unread_word);
  mem::image::read(loaded_image(kTextPageImage), data.text_page);
  for (int m = 0; m < 70; ++m) mem::image::read(loaded_image(kMissionRulesImage + m * kMissionRuleBytes), data.mission_eligible[m]);
}
}  // namespace

GAME_STATE_DATA(4, "red", data, kMovedData, load_data);

}  // namespace game::red
