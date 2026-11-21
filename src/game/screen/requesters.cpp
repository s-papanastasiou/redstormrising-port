// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The library's alerts and disk requesters (all five modules): a
// recoverable alert with the text, the fatal error that shows one and ends
// the game (the loader) or the module, and the requester asking for a
// disk until its volume can be locked.
#include "game/common/strings.hpp"
#include "game/input/calls.hpp"
#include "game/input/vars.hpp"
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/redstorm/calls.hpp"
#include "game/screen/calls.hpp"
#include "game/screen/vars.hpp"
#include "game/ui/state.hpp"

#include <cstring>
#include <string>

namespace game::screen {

namespace {
// An AlertMessage: x (a word), y (a byte), the text, a continuation byte
// (0: the last line).
constexpr uint32_t kRecoveryAlert = 0;
constexpr int32_t kAlertHeight = 40;
}  // namespace

// $31ab2 (RedStorm $21bc2, Title $238fa, RedStart $23bf2, Red $27b5a):
// a recoverable alert, 40 lines high, of `text` and the tail.
void alert(const char* text) {
  // One line of the AlertMessage at (kAlertX, kAlertY): the text and
  // the tail.
  std::string line = text;
  line += alert_tail.str();
  os::display_alert(kRecoveryAlert, line.c_str(), kAlertHeight);
}
ORIGINAL_INSIDE(alert, 0x31ab2, 0x21bc2, 0x238fa, 0x23bf2, 0x27b5a);

// $31b06 (RedStorm $21c16, Title $2394e, RedStart $23c46, Red $27bae):
// the alert (if there is a text), then the end: the loader quits the game;
// a module sets the quit flag and ends with its exit work. (The original
// tells the two apart by whether the running task is the loader's: true
// on the Amiga only in the loader, whose own module copy called quit_game,
// the others' calls there being unlinked; here every module runs in the
// loader's process, so the running module decides. The module's result
// was a stale stack word: 0 here.)
void fatal_error(const char* text) {
  if (text) alert(text);
  if (runtime::module_index() == runtime::kRedStormModule) {
    redstorm::quit_game();
  } else {
    ui::state().quit_key = 1;
    common::module_exit(0);
  }
  common::c_exit(0);
}
ORIGINAL_INSIDE(fatal_error, 0x31b06, 0x21c16, 0x2394e, 0x23c46, 0x27bae);

// $31ed4 (RedStorm $21fe4, Title $23d1c, RedStart $24014, Red $27f7c): a
// requester on the bitmap not shown ("Please Insert Disk:" and the
// volume's name in quotes, with "Press a Key to Cancel." if the player may)
// until `volume` can be locked (the lock into *lock) or a key or the
// select button cancels; the font, the palette and the bitmaps drawn to
// and shown are restored afterwards.
void disk_requester(const char* volume, uint32_t& lock, int16_t can_cancel) {
  const uint32_t rp = (ui::state().rastport).addr;
  const uint32_t font = (ui::state().rastport)->font;
  const int16_t shown = shown_bitmap(), drawing = drawing_bitmap();
  const int16_t other = shown == 0 ? int16_t(1) : int16_t(0);
  be16 palette[kPaletteWords];
  const uint16_t* colours = ui::state().screen->viewport.colormap.table;
  for (int i = 0; i < kPaletteWords; ++i) palette[i] = colours[i];
  os::set_font(rp, ui::state().fonts[kRequesterFont]);
  set_palette(requester_palette.array(kPaletteWords));
  draw_to(other);
  clear_screen();
  os::set_apen(rp, kRequesterFrame);
  os::move(rp, 40, 40);
  os::draw(rp, 280, 40);
  os::draw(rp, 280, 160);
  os::draw(rp, 40, 160);
  os::draw(rp, 40, 40);
  os::set_apen(rp, kRequesterText);
  auto line = [&](int32_t x, int32_t y, const char* text) {
    os::move(rp, x, y);
    os::text(rp, text, int32_t(std::strlen(text)));
  };
  line(100, 80, insert_disk_text.str());
  const std::string name = std::string(open_quote.str()) + volume + close_quote.str();
  line(110, 100, name.c_str());
  line(110, 120, any_drive_text.str());
  if (can_cancel) line(90, 180, cancel_text.str());
  show_bitmap(other);
  input::flush_keys();
  while (!lock) {
    lock = os::lock(volume, os::ACCESS_READ);
    wait_tof();
    wait_tof();
    if (can_cancel && (input::getkey() || ui::state().select_button)) break;
  }
  os::set_font(rp, font);
  set_palette(palette);
  draw_to(drawing);
  show_bitmap(shown);
}
ORIGINAL_INSIDE(disk_requester, 0x31ed4, 0x21fe4, 0x23d1c, 0x24014, 0x27f7c);

namespace {
// The volume locked (and let go), the requester (without cancelling)
// until it can be.
void need_volume(const char* volume, const char* name) {
  uint32_t lock = os::lock(volume, os::ACCESS_READ);
  if (!lock) disk_requester(name, lock, 0);
  os::unlock(lock);
}
}  // namespace

// $31e48 / $31e8e and the copies in every module: RedStormA: / RedStormB:
// asked for.
void need_disk_a() { need_volume(volume_a.str(), volume_a_name.str()); }
ORIGINAL(need_disk_a, 0x31e48, 0x21f58, 0x23c90, 0x23f88, 0x27ef0);
void need_disk() { need_volume(volume_b.str(), volume_b_name.str()); }
ORIGINAL(need_disk, 0x31e8e, 0x21f9e, 0x23cd6, 0x23fce, 0x27f36);

}  // namespace game::screen
