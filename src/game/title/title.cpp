// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Title: the opening pictures and the title sequence, the first module
// the loader runs.
#include "game/audio/calls.hpp"
#include "game/common/strings.hpp"
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/screen/calls.hpp"
#include "game/screen/vars.hpp"
#include "game/title/calls.hpp"
#include "game/title/vars.hpp"

namespace game::title {

// $201b0: the Title module: the first opening picture, the title sequence
// with its sound (a key ends it), then the second opening picture drawn
// into the hidden bitmap and shown; that bitmap goes to the next module.
void title_main() {
  common::module_start();
  screen::front_screen();
  screen::opening_picture(kFirstOpening, 0);
  screen::show_bitmap(0);
  audio::load_samples1();
  screen::set_palette(title_palette.array(screen::kPaletteWords));
  audio::play_effect(kSequenceEffect);
  screen::play_sequence(screen::kTitleSequence, screen::kNoVariant);
  audio::play_effect(kAfterEffect);
  audio::free_samples1();
  const int16_t back = screen::shown_bitmap() == 0 ? int16_t(1) : int16_t(0);
  screen::draw_to(back);
  screen::opening_picture(kSecondOpening, back);
  screen::show_bitmap(back);
  common::module_exit(back);
}
ORIGINAL(title_main, 0, 0, 0x201b0, 0, 0);

}  // namespace game::title
