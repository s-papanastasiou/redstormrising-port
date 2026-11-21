// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Storm (the battle module): the battle's displays state, keys, contact
// selection and the battle log.
#include "game/audio/calls.hpp"
#include "game/common/math.hpp"
#include "game/common/strings.hpp"
#include "game/common/vars.hpp"
#include "game/input/calls.hpp"
#include "game/input/vars.hpp"
#include "game/mem/register.hpp"
#include "game/screen/calls.hpp"
#include "game/storm/calls.hpp"
#include "game/platform/platform.hpp"
#include "game/storm/vars.hpp"
#include "game/ui/state.hpp"
#include "game/storm/texts.hpp"

#include <cstring>

#include <algorithm>

namespace game::storm {

namespace {
bool exists(const Object& o) { return o.state || o.flags; }
}  // namespace

// $241ae: whether (x, y) is in the screen area of the tactical displays.
int16_t in_map_area(int16_t x, int16_t y) { return x >= 73 && x <= 303 && y >= 27 && y <= 167 ? 1 : 0; }
ORIGINAL_IN("storm", 0x241ae, in_map_area);

// $241de: a marker of kind 0-3 (its colour) at (x, y), inside the map.
// Other kinds take their colour from a register of the caller in the
// original; here they draw in colour 0.
void mark(int16_t kind, int16_t x, int16_t y) {
  static constexpr int16_t kColours[] = {kPenCyan, kPenLightGrey, kPenLightBlue, kPenGreen};
  const int16_t colour = kind >= 0 && kind < 4 ? kColours[kind] : kPenBlack;
  if (in_map_area(x, y)) screen::plot_colour(x, y, colour);
}
ORIGINAL_IN("storm", 0x241de, mark);

// $2cfd0: whether the selected contact can be attacked now (fire_ready).
void update_ready() {
  bool ready;
  if (battle.paused) {
    ready = false;
  } else if (battle.display_mode == kDisplayPeriscope) {
    ready = true;
  } else if (battle.display_mode != 1 && battle.display_mode != 3 && battle.display_mode != 5) {
    ready = false;
  } else if (battle.aiming) {
    ready = true;
  } else if (battle.sub_mode != kPageTorpedo || battle.selected_object == -1) {
    ready = false;
  } else {
    const Object& o = objects[battle.selected_object];
    ready = (o.flags & kRunning) && (o.flags & kWired) && o.weapon == kMark48;
  }
  battle.fire_ready = ready ? 1 : 0;
}
ORIGINAL_IN("storm", 0x2cfd0, update_ready);
ORIGINAL_IN("storm", 0x25638, update_ready);  // a jump to it

// $24640: the keys waiting, each handled (the last result is returned,
// 1 without keys); a finished number entry clears the status line.
int16_t handle_keys() {
  int16_t result = 1;
  update_ready();
  while (const int16_t key = input::getkey()) result = process_key(key);
  if (result == 1 && battle.entry_mode && battle.typed_frames > 50) {
    battle.entry_mode = 0;
    clear_status();
  }
  return result;
}
ORIGINAL_IN("storm", 0x24640, handle_keys);

// $2b500, $2439c: sub-mode 3 or 5, remembering the current one.
void enter_mode3() {
  battle.saved_sub_mode = battle.sub_mode;
  battle.sub_mode = kPageDamage;
  battle.redraw_left = 1;
}
ORIGINAL_IN("storm", 0x2b500, enter_mode3);
void enter_mode5() {
  battle.saved_sub_mode = battle.sub_mode;
  battle.sub_mode = kPageThreats;
  battle.redraw_left = 1;
}
ORIGINAL_IN("storm", 0x2439c, enter_mode5);

// $2dc9a: an object into the battle log. Returns 1 when the log is full.
int16_t log_event(int16_t n) {
  const int16_t i = battle.log_count;
  battle.log_count += 1;
  const Object& o = objects[n];
  LogEntry& e = battle_log[i];
  e.what = uint16_t((n < 4 ? kLogWeapon : kLogObject) | o.id);
  e.x = o.x;
  e.y = o.y;
  return i > kLogFull ? 1 : 0;
}
ORIGINAL_IN("storm", 0x2dc9a, log_event);

// $2db82: a ship into the battle log with its events (hit, damaged,
// sunk; sunk ends its events, and the player's loss is noted). Returns 1
// when the log is full.
int16_t log_ship_event(int16_t n) {
  const int16_t i = battle.log_count;
  battle.log_count += 1;
  Ship& s = ships[n];
  LogEntry& e = battle_log[i];
  e.what = uint16_t((n == 0 ? kLogOwn : kLogShip) | s.id);
  if (s.events & kEventLaunched) {
    e.what |= kLogLaunched;
    s.events &= uint8_t(~kEventLaunched);
  }
  if (s.events & kEventDamaged) {
    e.what |= kLogDamaged;
    s.events &= uint8_t(~kEventDamaged);
  }
  if (s.events & kEventSunk) {
    s.events = 0;
    s.flags = 0;
    e.what |= kLogSunk;
    if (n == 0) battle.damage_word |= kDamageLost;
  }
  e.x = s.x;
  e.y = s.y;
  return i > kLogFull ? 1 : 0;
}
ORIGINAL_IN("storm", 0x2db82, log_ship_event);

// $2daf6: every active ship and object into the battle log, then an end
// mark; when the log is full it starts again from the beginning.
void log_all() {
  int16_t full = 0;
  for (int16_t i = 0; i < kShips; ++i)
    if (ships[i].flags & kAfloat) full = int16_t(full + log_ship_event(i));
  for (int16_t i = 0; i < kObjects; ++i)
    if (exists(objects[i])) full = int16_t(full + log_event(i));
  battle_log[battle.log_count].what = 0;
  if (!full) return;
  battle.log_wrapped = 1;
  battle.log_end = battle.log_count;
  battle.log_count = 0;
}
ORIGINAL_IN("storm", 0x2daf6, log_all);

// $290e2: unless the selected contact still exists (outside sub-mode 4),
// the next contact (cyclically) that exists and can be attacked; -1 if
// none.
void select_next_contact() {
  int16_t sel = battle.selected_object;
  if (sel >= 0 && exists(objects[sel]) && battle.sub_mode != kPageTorpedo) return;
  int16_t k = 0;
  for (;; ++k) {
    sel = int16_t((sel + 1) % kObjects);
    if (k >= kObjects) break;
    const Object& o = objects[sel];
    if (o.kind != kPlayerWeapon || !exists(o)) continue;
    if (o.weapon == kMark48 || o.weapon == kSeaLance) break;
  }
  battle.selected_object = k == kObjects ? int16_t(-1) : sel;
}
ORIGINAL_IN("storm", 0x290e2, select_next_contact);

// $2172c: the next ship heard or seen (ships 1-7 in turn; an aircraft
// only while the player is shallower than 56 feet; -1: none) is selected
// (from the one after the selection on page 1), and page 1 shown.
void next_contact() {
  if (battle.sub_mode == kPageContacts) battle.selected_ship += 1;
  int16_t n = battle.selected_ship;
  battle.selected_ship = 0;
  for (int16_t tries = 0; battle.selected_ship == 0;) {
    const Ship& s = ships[n];
    if ((s.flags & kAfloat) && ((s.flags_7 & kPlayerContact) || s.noise != 0) &&
        (ship_types[s.id].kind != kAircraftKind || ships[0].depth < kPeriscopeDepth)) {
      battle.selected_ship = n;
      if (battle.display_mode == kDisplayLibrary) sea_shades();
    }
    if (++n >= kShips) n = 1;
    if (++tries >= kShips) battle.selected_ship = -1;
  }
  if (battle.sub_mode != kPageContacts) battle.saved_sub_mode = battle.sub_mode;
  battle.sub_mode = kPageContacts;
  battle.redraw_left = 1;
}
ORIGINAL_IN("storm", 0x2172c, next_contact);

// $25a86: the depth of the sea at (x, y): by the sea bed's level (mode
// 0), from the table (mode 1), else 9999.
int16_t depth_at(int16_t x, int16_t y) {
  if (battle.depth_mode == 0) return int16_t((area_level(x, y) + 3) * 100 + 300);
  if (battle.depth_mode == 1) return ice_depths[area_level(x, y)];
  return kNoDepth;
}
ORIGINAL_IN("storm", 0x25a86, depth_at);

// $23604: the sea bed's level at a map point: the average of the 3 x 3
// cells there, -3..4.
int16_t area_level(int16_t x, int16_t y) {
  // The sea bed is Storm's own program read as terrain: its bytes, so it
  // depends on where Storm was loaded (its relocated addresses);
  // --reference-terrain takes them as the reference recordings had them.
  const uint8_t* reference = platform::reference_image();
  const uint32_t from_block = sea_bed.addr() - platform::module_block();
  auto sea_bed_at = [&](int16_t i) -> uint8_t {
    return reference ? reference[from_block + uint32_t(int32_t(i))] : sea_bed[i];
  };
  int16_t cell = int16_t(((x >> 2) & kSeaBedRowMask) + ((y >> 8) & 63));
  int16_t sum = 0;
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) sum = int16_t(sum + (sea_bed_at(int16_t(battle.sea_bed_base + cell + c)) & 7));
    cell = int16_t(cell + kSeaBedRow);
  }
  return common::clamp(int16_t((sum >> 2) - 5), -3, 4);
}
ORIGINAL_IN("storm", 0x23604, area_level);

// $212ea: the battle's outcome into the campaign's record: damage, tubes,
// weapons left and each enemy ship's state.
void save_battle_state() {
  BattleRecord& r = *battle_record();
  r.damage = int16_t(battle.damage_word);
  for (int16_t i = 0; i < 8; ++i) r.tubes[i] = battle.tube_contents[i];
  for (int16_t i = 0; i < 7; ++i) r.weapons[i] = battle.weapon_counts[i + 1];
  for (int16_t i = 0; i < r.enemy_ships; ++i) {
    const Ship& s = ships[i + 1];
    r.ship_state[i] = !(s.flags & kAfloat) ? int16_t(2) : (s.flags & kDamaged) ? int16_t(1) : int16_t(0);
  }
}
ORIGINAL_IN("storm", 0x212ea, save_battle_state);

// $213fe: the screen cleared on both bitmaps, the front one shown while
// picture 10 loads (into bitmap 0), then the back one shown again.
void fresh_screen() {
  screen::draw_to(*back_bitmap);
  screen::fill_rect_colour(0, 0, 319, 199, kPenBlack);
  screen::blit_bitmaps(*back_bitmap, 0, 0, *front_bitmap, 0, 0, 320, 200);
  screen::draw_to(*front_bitmap);
  screen::show_bitmap(*front_bitmap);
  screen::load_picture(10, 0);
  screen::draw_to(*back_bitmap);
  screen::show_bitmap(*back_bitmap);
}
ORIGINAL_IN("storm", 0x213fe, fresh_screen);

// $255d6: the pointer hidden (unless kept), and a pending status message
// cleared.
void refresh_status() {
  if (!ui::state().pointer_kept) screen::hide_pointer();
  if (!battle.aiming) return;
  battle.aiming = 0;
  clear_status();
}
ORIGINAL_IN("storm", 0x255d6, refresh_status);

// $255f0: the same without clearing the status line.
void refresh_status2() {
  if (!ui::state().pointer_kept) screen::hide_pointer();
  battle.aiming = 0;
}
ORIGINAL_IN("storm", 0x255f0, refresh_status2);

// $2d068: n frames.
void wait_frames(int16_t n) {
  for (uint16_t k = uint16_t(n); k != 0; --k) screen::wait_tof();
}
ORIGINAL_IN("storm", 0x2d068, wait_frames);

// $2d110: a number entry starts with message i on the status line.
void start_message(int16_t i) {
  sight.question = uint16_t(i);
  status_message(const_text(start_messages[i]), kPenLightGrey, 0);
  battle.entry_mode = uint16_t(i);
  battle.typed_length = 0;
  battle.typed_frames = 0;
  for (int k = 0; k < 4; ++k) texts().typed_digits[k] = 0;
}
ORIGINAL_IN("storm", 0x2d110, start_message);

// $2d2f2: the number typed (1-3 digits; 0 otherwise).
int16_t input_number() {
  const char* s = texts().typed_digits;
  auto digit = [&](int k) { return int16_t(s[k] - '0'); };
  switch (int32_t(std::strlen(texts().typed_digits))) {
    case 1: return digit(0);
    case 2: return int16_t(digit(0) * 10 + digit(1));
    case 3: return int16_t(digit(0) * 100 + digit(1) * 10 + digit(2));
    default: return 0;
  }
}
ORIGINAL_IN("storm", 0x2d2f2, input_number);

// $2d158: a key of a number entry: digits are added (a full field counts
// as Return after a moment), Backspace removes one, Escape cancels, Return
// gives the order (course, depth or the tube to unload). Other keys, or
// Backspace and Return with nothing typed, beep.
void order_key(int16_t key) {
  constexpr char kBackspace = 8, kReturn = 13, kEscape = 27;
  constexpr int16_t kTextY = 177, kColour = kPenLightGrey;
  char k = char(key);
  battle.typed_frames = 0;
  const int16_t mode = battle.entry_mode;
  auto beep = [] { audio::play_effect(kEffectBeep); };
  if (k >= '0' && k <= '9' && battle.typed_length < entry_lengths[mode]) {
    texts().typed_digits[battle.typed_length] = char(k);
    text_shadow(texts().typed_digits, entry_x[mode], kTextY, kColour);
    battle.typed_length += 1;
    if (battle.typed_length != entry_lengths[mode]) return;
    wait_frames(20);
    k = kReturn;
  } else if (k != kBackspace && k != kReturn && k != kEscape) {
    beep();
    return;
  }
  if (k == kBackspace) {
    if (battle.typed_length == 0) return beep();
    texts().typed_digits[battle.typed_length - 1] = 0;
    const int16_t x = entry_x[mode];
    screen::fill_rect_colour(x, kStatusTop, int16_t(x + 24), kStatusBottom, kPenBlack);
    text_shadow(texts().typed_digits, entry_x[mode], kTextY, kColour);
    battle.typed_length -= 1;
    return;
  }
  if (k == kEscape) {
    battle.entry_mode = 0;
    clear_status();
    beep();
    return;
  }
  if (battle.typed_length == 0) return beep();
  clear_status();
  const int16_t n = input_number();
  if (battle.entry_mode == kEntryCourse) order_course(n);
  else if (battle.entry_mode == kEntryDepth) order_depth(n);
  else if (battle.entry_mode == kEntryTube) unload_tube(n);
  battle.entry_mode = 0;
}
ORIGINAL_IN("storm", 0x2d158, order_key);

// $223a8: a random sea: type 14-17, and two more values.
void random_sea() {
  sight.sea_type = int16_t(common::rnd_inclusive(3) + 14);
  sight.wave_y = int16_t(common::rnd_inclusive(7) + 1);
  sight.wave_x = common::rnd_inclusive(31);
}
ORIGINAL_IN("storm", 0x223a8, random_sea);

// $223d6: random conditions: sea type 19-22 and two more values.
void random_conditions() {
  sight.sea_type = int16_t(common::rnd_inclusive(3) + 19);
  sight.wave_y = int16_t(common::rnd_inclusive(7) + 10);
  sight.wave_x = common::rnd_inclusive(31);
}
ORIGINAL_IN("storm", 0x223d6, random_conditions);

// $28bae: a blow to the player's submarine: a sound, and (unless damage
// is off) 1 to setting + 1 hits, each on a system not yet damaged (with
// its event): sonar, ..., the tubes (only three are left), the engines
// (it stops). With every system damaged and hits left, it sinks.
void random_damage() {
  if (!battle.action_track) audio::play_effect(kEffectBlow);
  if (battle.no_damage) return;
  Ship& own = ships[0];
  int16_t hits = int16_t(common::rnd(int16_t(battle.difficulty + 1)) + 1);
  static constexpr uint8_t kSystemBit[] = {1, 0, 2, 3};  // by event
  while (hits != 0 && (battle.damage_word & kAllSystems) != kAllSystems) {
    const int16_t k = common::rnd(4);
    const uint8_t bit = uint8_t(1 << kSystemBit[k]);
    if (battle.damage_word & bit) continue;
    battle.event_flags[k] = 1;
    own.events |= kEventDamaged;
    battle.damage_word |= bit;
    if (k == 2) battle.tube_count = 3;
    if (k == 3) {
      own.speed_set = 0;
      own.speed = 0;
    }
    --hits;
  }
  if ((battle.damage_word & kAllSystems) == kAllSystems && hits > 0) own.events |= kEventSunk;
}
ORIGINAL_IN("storm", 0x28bae, random_damage);

// $22de8: the sonar shades of the selected ship's type: 32 pens by level
// (as shade_column draws them, with the type's place in the list as the
// phase), and the box under them.
void sea_shades() {
  const int16_t id = int16_t(ships[battle.selected_ship].id);
  int16_t k = 0;
  while (k < kShipList && ship_list[k] != uint16_t(id)) ++k;
  ShadeLevels shades(ship_types[id], k);
  for (int16_t i = 0; i < kShadeLines; ++i) shade_pens[i] = shades.next_pen(i);
  battle.noise_half = 0;
  battle.noise_row = 0;
  screen::filled_box(168, 140, 211, 164, kPenLightBlue);
}
ORIGINAL_IN("storm", 0x22de8, sea_shades);

// $22f38: four dots of sonar noise in the box, one row every two calls:
// a random shade, its place blurred by the selected ship's quietness.
void sea_noise() {
  if (battle.paused) return;
  const int16_t spread = int16_t((256 - ships[battle.selected_ship].noise) / 32);
  for (int16_t k = 0; k < 4; ++k) {
    int16_t r = common::rnd_inclusive(31);
    screen::set_apen(shade_pens[r]);
    const int16_t roll = common::rnd(128);
    if (ships[battle.selected_ship].noise < roll) {
      const int16_t shift = common::rnd(int16_t(2 * spread + 1));
      r = common::clamp(int16_t(r + shift - spread), 0, 31);
    }
    screen::plot(int16_t(r + 174), int16_t(145 + battle.noise_row));
  }
  battle.noise_half += 1;
  if (battle.noise_half != 2) return;
  battle.noise_half = 0;
  battle.noise_row += 1;
  if (battle.noise_row != 16) return;
  battle.noise_row = 0;
}
ORIGINAL_IN("storm", 0x22f38, sea_noise);

// $2c1ac: the depth chart: a digit per map cell, the sea bed's level
// there (as a character of the chart's font: the sea bed, the ice or
// other), then the scale's ticks.
void depth_chart() {
  char base = 25;
  int16_t colour = kPenIndigo;
  if (battle.depth_mode == 0) {
    base = 13;
  } else if (battle.depth_mode == 1) {
    base = 1;
    colour = kPenCyan;
  }
  texts().text_buffer[1] = 0;
  text_style->font = 3;
  for (int16_t row = 3; row < 20; ++row) {
    for (int16_t col = 9; col < 37; ++col) {
      const int16_t x = int16_t(battle.view_x + int16_t(int16_t(col * 8 - 183) << battle.scale));
      const int16_t y = int16_t(battle.view_y + int16_t(int16_t(row * 8 - 93) << battle.scale));
      const char* digits = common::format_number(int16_t(area_level(x, y) + 3), 1);
      texts().text_buffer[0] = char(digits[0] + base - '0');
      text_at_cell(texts().text_buffer, col, row, colour);
    }
  }
  text_style->font = 1;
  draw_ticks();
}
ORIGINAL_IN("storm", 0x2c1ac, depth_chart);

// $28cc0: each of four events that happened: its message, sub-mode 3.
void report_events() {
  const Const<char>* texts[] = {&event_texts_a, &event_texts_b, &event_texts_c, &event_texts_d};
  for (int i = 0; i < 4; ++i) {
    if (!battle.event_flags[i]) continue;
    battle.event_flags[i] = 0;
    status_message(texts[i]->str(), kPenRed, 1);
    battle.sub_mode = kPageDamage;
    battle.redraw_left = 1;
  }
}
ORIGINAL_IN("storm", 0x28cc0, report_events);

// $2ba26: a display's title (when the mode changed) and its background.
void show_mode(int16_t mode, int16_t colour) {
  if (battle.shown_mode != mode) {
    screen::fill_rect_colour(101, 16, 273, 23, kPenBlack);
    const char* name = const_text(mode_names[mode]);
    text_shadow(name, int16_t(187 - 3 * int16_t(std::strlen(name))), 16, kPenYellow);
    battle.shown_mode = battle.display_mode;
  }
  screen::fill_rect_colour(kStatusLeft, 27, kStatusRight, 167, colour);
}
ORIGINAL_IN("storm", 0x2ba26, show_mode);

// $2ae04: a two-line gauge on rows row+27, row+28 from x 127.
void gauge(int16_t value, int16_t row, int16_t colour) {
  const int16_t level = std::clamp<int16_t>(int16_t(value + 116), 0, 199);
  const int16_t y1 = int16_t(row + 27), y2 = int16_t(row + 28);
  if (level > 26) {
    screen::line_colour(127, y1, int16_t(level + 100), y1, colour);
    screen::line_colour(127, y2, int16_t(level + 100), y2, int16_t(colour + 32));
    screen::line_colour(299, y1, int16_t(level + 101), y1, kPenBlack);
    screen::line_colour(299, y2, int16_t(level + 101), y2, kPenBlack);
  } else {
    screen::line_colour(127, y1, 299, y1, kPenBlack);
    screen::line_colour(127, y2, 299, y2, kPenBlack);
  }
}
ORIGINAL_IN("storm", 0x2ae04, gauge);

namespace {
char* text() { return texts().text_buffer; }
}  // namespace

// $2531e: the ordered speed moves by `steps` of six levels of the
// maximum (halved with engine damage), with a message.
void change_speed(int16_t steps) {
  Ship& own = ships[0];
  int16_t max = ship_types[own.id].max_speed;
  if (battle.damage_word & kDamagePropulsion) max = int16_t(max / 2);
  int16_t level = int16_t(int16_t((own.speed_set + 1) * 6) / max);
  level = std::clamp<int16_t>(int16_t(level + steps), 0, 6);
  own.speed_set = int16_t(int16_t(max * level) / 6);
  if (steps) {
    string_copy(text(), text_speed.str());
    string_append(text(), common::format_number(level, 1));
    status_message(text(), kMessageColour, 1);
  }
  battle.redraw_helm = 1;
}
ORIGINAL_IN("storm", 0x2531e, change_speed);

// $253e0: the rudder moves by dir (-3..3); against its side it is
// centred. With a message; an ordered course ends.
void change_rudder(int16_t dir) {
  if (common::abs_saturated(int16_t(battle.rudder + dir)) > 3) return;
  const int16_t r = battle.rudder;
  if ((dir > 0 && r < 0) || (dir < 0 && r > 0)) {
    battle.rudder = 0;
    string_copy(text(), text_amidships.str());
  } else {
    battle.rudder = std::clamp<int16_t>(int16_t(r + dir), -3, 3);
    string_copy(text(), common::format_number(common::abs_saturated(int16_t(battle.rudder * 5)), 2));
    string_append(text(), text_rudder.str());
    string_append(text(), (dir < 0 ? text_left : text_right).addr());
    string_append(text(), text_degrees.str());
  }
  status_message(text(), kMessageColour, 1);
  ships[0].course = 0;
  battle.redraw_helm = 1;
}
ORIGINAL_IN("storm", 0x253e0, change_rudder);

// $25232: the display scale (0-7, kept per mode) moves by dir in the
// tactical modes, with a message.
void change_scale(int16_t dir) {
  const int16_t m = battle.display_mode;
  if (m != 1 && m != 5 && m != 3) return;
  const int16_t sc = battle.scale;
  if ((dir > 0 && sc == 7) || (dir < 0 && sc == 0)) {
    status_message((dir < 0 ? text_scale_min : text_scale_max).str(), kMessageColour, 0);
    status_if_flag();
    return;
  }
  battle.scale = std::clamp<int16_t>(int16_t(sc + dir), 0, 7);
  battle.scales_by_mode[m] = battle.scale;
  string_copy(text(), (dir < 0 ? text_zoom_in : text_zoom_out).addr());
  string_append(text(), text_scale.str());
  string_append(text(), common::format_number(battle.scale, 1));
  status_message(text(), kMessageColour, 0);
  status_if_flag();
  battle.redraw_setup = 1;
}
ORIGINAL_IN("storm", 0x25232, change_scale);

// $292f2: load a weapon into the first empty tube (with its load time
// and a message); none left: a message; all tubes full: a number entry
// for the tube to unload.
void load_tube(int16_t weapon) {
  if (battle.weapon_counts[weapon] == 0) {
    status_message(text_no_reload.str(), kPenWhite, 1);
    return;
  }
  int16_t i = 0;
  while (battle.tube_contents[i] != 0 && i < battle.tube_count - 1) ++i;
  if (battle.tube_contents[i] != 0) {
    battle.pending_weapon = weapon;
    start_message(3);
    if (battle.sub_mode != kPageWeapons) {
      battle.sub_mode = kPageWeapons;
      battle.redraw_left = 1;
    }
    return;
  }
  battle.tube_contents[i] = weapon;
  battle.load_times[i] = kLoadTime;
  battle.weapon_counts[weapon] -= 1;
  string_copy(text(), text_loading.str());
  string_append(text(), const_text(weapon_names[weapon]));
  string_append(text(), text_in_tube.str());
  string_append(text(), common::format_number(int16_t(i + 1), 1));
  string_append(text(), text_sir.str());
  status_message(text(), kMessageColour, 1);
  if (battle.sub_mode == kPageReloads) battle.redraw_left = 1;
}
ORIGINAL_IN("storm", 0x292f2, load_tube);

// $2553e: the answer to "unload which tube?": tube n (from 1) is emptied
// (its weapon back in store) and the pending weapon loaded instead.
void unload_tube(int16_t n) {
  if (n > battle.tube_count || n == 0) {
    battle.entry_mode = 0;
    string_copy(text(), text_unable_unload.str());
    string_append(text(), common::format_number(n, 1));
    status_message(text(), kPenWhite, 1);
    return;
  }
  int16_t& tube = battle.tube_contents[n - 1];
  const int16_t weapon = int16_t(tube & 127);
  tube = 0;
  battle.weapon_counts[weapon] += 1;
  load_tube(battle.pending_weapon);
}
ORIGINAL_IN("storm", 0x2553e, unload_tube);

// $2c0ac: the scale's ticks on the map's frame, 512 >> scale pixels
// apart from the centre: short lines on the top and bottom edges, dots on
// the left and right edges.
void draw_ticks() {
  constexpr int16_t kCentreX = 188, kCentreY = 97, kColour = kPenLightGrey;
  const int16_t step = int16_t(512 >> battle.scale);
  for (int16_t d = 0; kCentreX + d < kStatusRight; d = int16_t(d + step)) {
    for (const int16_t x : {int16_t(kCentreX + d), int16_t(kCentreX - d)}) {
      screen::line_colour(x, 27, x, 28, kColour);
      screen::line_colour(x, 166, x, 167, kColour);
    }
  }
  for (int16_t d = 0; kCentreY + d < 167; d = int16_t(d + step)) {
    for (const int16_t y : {int16_t(kCentreY + d), int16_t(kCentreY - d)}) {
      screen::plot_colour(kStatusLeft, y, kColour);
      screen::plot_colour(kStatusRight, y, kColour);
    }
  }
}
ORIGINAL_IN("storm", 0x2c0ac, draw_ticks);

// $2d540: the map of the battle log (display 10), centred on the entry
// being shown; zoomed in beyond scale 3 the centre moves half a view
// towards the nearest of the ship entries that follow it.
void map_view() {
  battle.display_mode = 10;
  show_mode(10, 0);
  const int16_t at = battle.log_view_index;
  const LogEntry& here = battle_log[at];
  int16_t nearest = -1, best = kNoRange, bearing = 0;
  for (int16_t i = int16_t(at + 1); battle_log[i].what & kLogShip; ++i) {
    const LogEntry& e = battle_log[i];
    course_to(e.x, e.y, here.x, here.y);
    if (sight.course_range < best) {
      best = sight.course_range;
      bearing = sight.course_bearing;
      nearest = i;
    }
  }
  battle.view_x = here.x;
  battle.view_y = here.y;
  if (nearest != -1 && battle.scale > 3) {
    const int16_t size = int16_t(80 << battle.scale);
    const int16_t towards = int16_t(bearing + kHalfTurn);
    battle.view_x += int16_t(common::sin_mul(towards, size) >> 1);
    battle.view_y -= int16_t(common::cos_mul(towards, size) >> 1);
  }
  tactical_background();
  screen::blit_bitmaps(*back_bitmap, kStatusLeft, 27, *front_bitmap, kStatusLeft, 27, 230, 140);
  clear_status();
}
ORIGINAL_IN("storm", 0x2d540, map_view);

// $2d408: the battle log replayed on the map until it is all shown or
// Escape; a wrapped log starts at the first of the player's entries after
// the newest ones. X and Z zoom out and in; when the scale changes, or a
// step asks for it, the map is redrawn with a message.
void replay_log() {
  battle.log_view_index = 0;
  if (battle.log_wrapped && battle.log_count < kLogLastStart) {
    int16_t i = battle.log_count;
    for (;;) {
      ++i;
      if (battle_log[i].what & kLogOwn) break;
      if (i > kLogEntries) {
        i = 0;
        break;
      }
    }
    battle.log_view_index = i;
  }
  map_view();
  int16_t zoom = 0;
  bool going = true;
  while (battle.log_view_index != battle.log_count && going) {
    switch (input::getkey() & 255) {
      case 27: going = false; break;
      case 'X':
      case 'x':
        if (battle.scale < 7) {
          battle.scale += 1;
          zoom = 1;
        }
        break;
      case 'Z':
      case 'z':
        if (battle.scale > 1) {
          battle.scale -= 1;
          zoom = -1;
        }
        break;
      default: break;
    }
    if (zoom != 0 || !replay_step()) {
      string_copy(text(), (zoom < 0 ? text_zoom : text_expand).addr());
      string_append(text(), text_to_scale.str());
      string_append(text(), common::format_number(battle.scale, 1));
      status_message(text(), kMessageColour, 0);
      map_view();
      replay_step();
      zoom = 0;
    }
    wait_frames(int16_t(*replay_delay << 3));
    if (battle.log_wrapped && battle.log_view_index == battle.log_end) battle.log_view_index = 0;
  }
}
ORIGINAL_IN("storm", 0x2d408, replay_log);

// $2d3c6: (unless difficulty) the replay of the battle log, ending any
// aiming or number entry first; the display is kept.
void replay_battle() {
  if (battle.difficulty) return;
  if (battle.aiming) {
    battle.aiming = 0;
    clear_status();
  }
  if (battle.entry_mode) {
    battle.entry_mode = 0;
    clear_status();
  }
  const int16_t display = battle.display_mode;
  replay_log();
  battle.display_mode = display;
  battle.redraw_setup = 1;
  clear_status();
  status_if_flag();
}
ORIGINAL_IN("storm", 0x2d3c6, replay_battle);

// $23078: display 5 with its scale, everything redrawn.
void enter_display5() {
  battle.display_mode = 5;
  battle.sub_mode = kPageThreats;
  battle.scale = battle.scales_by_mode[5];
  battle.redraw_setup = 1;
  battle.redraw_left = 1;
}
ORIGINAL_IN("storm", 0x23078, enter_display5);

// $236a0: display 1 with its scale.
void enter_display1() {
  battle.scale = battle.scales_by_mode[1];
  battle.display_mode = 1;
  battle.redraw_setup = 1;
}
ORIGINAL_IN("storm", 0x236a0, enter_display1);

// $20aaa: the identification list from the setting's ship list; entries
// 22-24 are kept for this battle's enemies, and enemies beyond the list
// take the free ones of them.
void setup_ship_list() {
  const ConstPtr<be16> source{ship_lists_by_era[battle.era]};
  for (int16_t k = 0; k < 32; ++k) ship_list[k] = source[k];
  for (int16_t k = kSpareFirst; k < kSpareEnd; ++k) ship_list[k] = uint16_t(ship_list[k] | kSpareFree);
  const BattleStart& b = *battle_start();
  for (int16_t e = 0; e < 5; ++e) {
    const int16_t t = b.enemy_types[e];
    if (t > kSpareFirst - 1 && t < kSpareEnd) ship_list[t] = uint16_t(ship_list[t] & ~kSpareFree);
  }
  for (int16_t e = 0; e < 5; ++e) {
    const int16_t t = b.enemy_types[e];
    if (t <= kSpareEnd - 1) continue;
    for (int16_t k = kSpareFirst; k < kSpareEnd; ++k) {
      if (ship_list[k] & kSpareFree) {
        ship_list[k] = source[t];
        break;
      }
    }
  }
  for (int16_t k = kSpareFirst; k < kSpareEnd; ++k) ship_list[k] = uint16_t(ship_list[k] & ~kSpareFree);
}
ORIGINAL_IN("storm", 0x20aaa, setup_ship_list);

namespace {
// The player's submarine from the campaign: its class, speed and
// heading; its depth at random (deep when fast, 150 feet by the sea bed).
void start_own_ship(Ship& s, const BattleStart& b) {
  s.side = 0;
  s.id = uint16_t(battle.own_class);
  s.events = 0;
  s.flags = kAfloat | kTowedArray;
  s.x = 0;
  s.y = 0;
  s.depth = int16_t(common::rnd(20) * 10 + 150);
  s.speed = b.own_speed;
  if (s.speed > 15) s.depth = 900;
  s.heading = contact_bearings[b.own_direction];
  s.speed_set = s.speed;
  s.course = s.heading;
  s.array_heading = s.heading;
  s.last_heading = s.heading;
  if (battle.depth_mode == 0) s.depth = 150;
  s.depth_set = s.depth;
}

// Enemy ship n (from 1) of the group: its type from the battle start, at
// two thirds of its top speed on the group's route; submarines at 250
// feet.
void start_enemy(Ship& s, int16_t n, const BattleStart& b) {
  s.side = 1;
  s.id = ship_list[b.enemy_types[n - 1]];
  s.events = 0;
  s.flags = kAfloat;
  s.x = 0;
  s.y = 0;
  const ShipType& t = ship_types[s.id];
  s.speed_set = int16_t(int16_t(t.max_speed << 2) / 6);
  s.speed = s.speed_set;
  s.heading = contact_bearings[b.route_direction];
  s.depth = t.kind == kSubmarineKind ? int16_t(250) : int16_t(0);
  s.depth_set = s.depth;
  s.course = s.heading;
  s.array_heading = s.heading;
  s.last_heading = s.heading;
}

void clear_ship(Ship& s) {
  s.side = 0;
  s.id = 0;
  s.events = 0;
  s.flags = 0;
  s.x = s.y = s.depth = s.speed = s.heading = 0;
  s.depth_set = s.speed_set = s.course = 0;
}

// What no ship starts with: contacts, marks, gauges, the last launch.
void clear_contact_state(Ship& s) {
  s.heard = 0;
  s.flags_7 = 0;
  s.mark_x = s.mark_y = s.mark_heading = 0;
  s.noise = s.bearing = s.range = 0;
  s.gauge_active = s.gauge_hull = s.gauge_towed = 0;
  s.heard_by = s.heard_signal = 0;
  s.gauge_46 = s.gauge_48 = s.gauge_50 = 0;
  s.hears_by = s.sonar_signal = 0;
  s.last_launch = -99;
}

// The enemies in formation around (cx, cy): submarines by speed, surface
// ships by toughness further apart.
void place_formation(const BattleStart& b, int16_t cx, int16_t cy) {
  for (int16_t n = 1; n < kShips; ++n) {
    Ship& s = ships[n];
    if (!(s.flags & kAfloat)) continue;
    const ShipType& t = ship_types[s.id];
    int16_t distance;
    if (t.kind == kSubmarineKind) distance = t.max_speed > 24 ? kFormationFast : kFormationSlow;
    else distance = t.toughness > 33 ? kFormationClose : kFormationWide;
    const int16_t angle = int16_t(s.heading + int16_t((2 * n - b.enemy_ships) << 12));
    s.x = int16_t(cx + common::sin_mul(angle, distance));
    s.y = int16_t(cy - common::cos_mul(angle, distance));
  }
}

// The submarine on the contact's bearing from a random enemy, from
// kStartRange closing by quarters until it hears one (the noise floor
// falling with the range); that one is selected. (With no damage it is
// placed once more at the last range.)
void place_submarine(const BattleStart& b) {
  Ship& own = ships[0];
  const int16_t k = int16_t(common::rnd(b.enemy_ships) + 1);
  const int16_t dir = contact_bearings[b.contact_direction];
  int16_t d = kStartRange, j = 1;
  for (; d > 255; d = int16_t(int16_t(d * 3) / 4)) {
    own.x = int16_t(ships[k].x + common::sin_mul(dir, d));
    own.y = int16_t(ships[k].y - common::cos_mul(dir, d));
    battle.noise_floor = int16_t(d / 200 + 25);
    for (j = 1; j < kShips; ++j) {
      if (ships[j].flags & kAfloat) {
        track_ship(j);
        hear(0, j);
      }
      if (ships[j].heard_signal > 0) break;
    }
    if (j < kShips && ships[j].heard_signal > 0) break;
  }
  if (battle.no_damage) {
    own.x = int16_t(ships[k].x + common::sin_mul(dir, d));
    own.y = int16_t(ships[k].y - common::cos_mul(dir, d));
  }
  battle.selected_ship = j;
}

// With a helicopter carrier in a group of fewer than five, a helicopter
// ahead of the group, unless rnd(difficulty + 1) is 0 (at difficulty 0:
// never).
void maybe_helicopter(const BattleStart& b, int16_t cx, int16_t cy) {
  bool helicopters = false;
  if (b.enemy_ships < 5)
    for (int16_t n = 1; n < b.enemy_ships + 1; ++n)
      if (ship_types[ships[n].id].weapons & kHasHelicopter) helicopters = true;
  if (!helicopters || !common::rnd(int16_t(battle.difficulty + 1))) return;
  Ship& h = ships[b.enemy_ships + 1];
  h.side = 1;
  h.id = 0;
  h.events = 0;
  h.flags = kAfloat;
  h.depth = -1;
  h.depth_set = -1;
  h.speed = 10;
  h.heading = contact_bearings[b.route_direction];
  h.speed_set = 20;
  h.x = int16_t(cx + common::sin_mul(h.heading, kHeloDistance));
  h.y = int16_t(cy - common::cos_mul(h.heading, kHeloDistance));
}
}  // namespace

// $20b8e: the ships at the start of a battle: the submarine, the enemies
// in formation around a random point, the submarine placed where it hears
// one, maybe a helicopter.
void setup_ships() {
  const BattleStart& b = *battle_start();
  battle.enemy_tactics = battle.difficulty ? int16_t(10) : int16_t(14);
  for (int16_t n = 0; n < kShips; ++n) {
    Ship& s = ships[n];
    if (n == 0) start_own_ship(s, b);
    else if (n < b.enemy_ships + 1) start_enemy(s, n, b);
    else clear_ship(s);
    clear_contact_state(s);
  }
  const int16_t cx = int16_t(common::rnd(kStartSpread) - kStartSpread / 2);
  const int16_t cy = int16_t(common::rnd(kStartSpread) - kStartSpread / 2);
  place_formation(b, cx, cy);
  Ship& own = ships[0];
  if (battle.no_damage) {
    own.speed_set = 15;
    own.speed = 15;
  }
  if ((battle.damage_word & kDamagePassiveSonar) && (battle.damage_word & kDamageTowedArray)) own.flags |= kDetected;
  place_submarine(b);
  maybe_helicopter(b, cx, cy);
}
ORIGINAL_IN("storm", 0x20b8e, setup_ships);

// $20774: a battle's state at its start: the displays, the log, the sea
// (from the campaign: its mode, the layer's depth, the ambient noise,
// the duct and layer at random), the ships, the tubes and weapons left,
// the torpedo's data by the submarine's class, and empty weapon and
// projectile slots.
void battle_init() {
  const BattleStart& b = *battle_start();
  const BattleRecord& r = *battle_record();
  battle.aircraft_attacks = 0;
  battle.time_rate = 6;
  battle.scales_by_mode[1] = 6;
  battle.scales_by_mode[3] = 3;
  battle.scales_by_mode[5] = 3;
  ui::state().quit_key = 0;
  battle.scale = 6;
  battle.named_ship = 0;
  battle.selected_ship = 0;
  battle.selected_object = -1;
  battle.shown_mode = -1;
  battle.display_mode = 1;
  battle.saved_sub_mode = -1;
  battle.sub_mode = kPageContacts;
  battle.helm_flag = 0;
  battle.view_x = -1;
  battle.view_y = -1;
  battle.last_enemy_launch = -99;
  sight.launch_x = 0;
  sight.launch_y = 0;
  battle.depth_chart_on = 0;
  battle.active_weapon = -1;
  battle.aiming = 0;
  battle.frame_count = 0;
  battle.rudder = 0;
  battle.quiet_frames = 0;
  battle.log_count = 0;
  battle.log_end = kLogFirstEnd;
  battle.log_wrapped = 0;
  battle.log_view_index = 0;
  battle.aircraft_shot_down = 0;
  battle.depth_mode = b.sea_mode;
  battle.damage_word = uint16_t(r.damage);
  battle.layer_depth = mode_layer_depths[battle.depth_mode];
  battle.ambient_noise = int16_t(ambient_by_mode[battle.depth_mode] + 3);
  if (battle.depth_mode == 1) battle.contact_flags |= kUnderIce;
  if (battle.depth_mode == 2) battle.contact_flags |= kDeepSea;
  battle.duct_strength = int16_t(common::rnd(4) + 2);
  battle.layer_strength = int16_t(common::rnd(3) + 3);
  if (battle.layer_strength == 5) battle.duct_strength = 5;
  setup_ship_list();
  setup_ships();
  for (int16_t t = 0; t < 8; ++t) battle.tube_contents[t] = r.tubes[t];
  for (int16_t w = 0; w < 7; ++w) battle.weapon_counts[w + 1] = r.weapons[w];
  ShipType& torpedo = torpedo_type();
  if (battle.own_class == kOwnClassQuiet) {
    battle.tube_count = 8;
    torpedo.noise = 26;
    torpedo.defence = -7;
  } else {
    battle.tube_count = 4;
    torpedo.noise = 27;
    torpedo.defence = -6;
  }
  torpedo.max_speed = battle.era ? int16_t(60) : int16_t(54);
  if (battle.damage_word & kDamageTubes) battle.tube_count = 3;
  for (int16_t i = 0; i < kObjects; ++i) {
    Object& o = objects[i];
    o = Object{};
    o.kind = i < 8 ? uint16_t(2) : uint16_t(3);
  }
  for (int16_t p = 0; p < kProjectiles; ++p) {
    Projectile& j = projectiles[p];
    j = Projectile{};
    j.kind = uint16_t(projectile_kinds[p]);
  }
}
ORIGINAL_IN("storm", 0x20774, battle_init);

namespace {
// A Y/N question on the display (two lines in the larger font).
void ask(const Const<char>& line1, const Const<char>& line2, int16_t x, int16_t y) {
  screen::hide_pointer();
  show_mode(battle.display_mode, 0);
  text_style->font = 2;
  ::game::storm::text(line1.str(), x, y, kPenYellow);
  ::game::storm::text(line2.str(), x, int16_t(y + 10), kPenWhite);
  text_style->font = 1;
}

// Waits for a key; returns it (character in the low byte, raw code in the
// high byte when there is no character).
int16_t wait_any_key() {
  int16_t key;
  do key = input::getkey(); while (!key);
  return key;
}
}  // namespace

// $20228: Storm's main: a battle from the campaign's record (the
// settings, the submarine's class), the general quarters report, then
// frames until it is over (no enemies or weapons left, or the submarine
// sunk) or quit: each frame the battle step, the displays, the keys and
// weapons (more often in real time, paced by the vertical blank). Now and
// then the crew asks whether to go on (the enemy opening the range, or
// long without contacts). At the end the replay is offered, then the
// after action report, and the outcome goes back to the campaign.
namespace {
bool quit() { return ui::state().quit_key != 0; }

// The battle's start: the screen, pointer and palette, the settings from
// the battle start (era, difficulty, damage, the submarine's class), the
// ships placed, the general quarters report, display 1.
void start_battle(const BattleStart& b) {
  screen::draw_to(0);
  screen::show_bitmap(0);
  screen::clear_screen();
  ui::state().pointer_kept = 0;
  screen::set_pointer(ui::state().pointer_7x7);
  screen::hide_pointer();
  sight.pending_effect = 0;
  battle.action_track = 1;
  battle.paused = 0;
  battle.entry_mode = 0;
  battle.aiming = 0;
  battle.time_rate = kRealTime;
  battle.sounds_on = 1;
  screen::set_palette(battle_palette.array(screen::kPaletteWords));
  battle.era = int16_t(b.era & 3);
  battle.difficulty = int16_t(b.difficulty & 3);
  battle.no_damage = b.level > 7 ? 1 : 0;
  static constexpr int16_t kClasses[] = {5, 7, 9, 10};
  const int16_t c = int16_t(b.class_6 >> 3);
  battle.own_class = c >= 0 && c < 4 ? kClasses[c] : kOwnClassQuiet;
  if ((battle.own_class == 5 || battle.own_class == 7) && battle.era == 3) battle.own_class += 1;
  battle.sea_bed_base = common::rnd(common::kAngleQuarter);  // 16384
  battle_init();
  general_quarters_report();
  fresh_screen();
  set_display(1, 1);
  battle.cycle8 = 0;
  battle.object_cycle = 0;
  battle.battle_over = 0;
  texts().last_status[0] = 0;
}

// The crew's question answered (after `ask`): while `pending()` holds,
// keys are read: N ends the battle, Y calls `yes()`, Alt-Q quits.
template <typename Pending, typename Yes>
void answer_question(Pending pending, Yes yes) {
  input::flush_keys();
  while (pending() && !battle.battle_over && !quit()) {
    const int16_t key = wait_any_key();
    const int16_t ch = int16_t(key & 255);
    if (ch) {
      if (ch == 'n' || ch == 'N') battle.battle_over = 1;
      if (ch == 'y' || ch == 'Y') {
        yes();
        battle.redraw_setup = 1;
        input::flush_keys();
      }
    } else if ((key >> 8) == kQuitKey) {
      ui::state().quit_key = 1;
    }
  }
}

// Every 64 frames: the nearest enemy ship's range (0 while a weapon
// runs); twice in a row increasing (and the player quiet), the crew asks
// whether to go on.
void check_range_opening() {
  sight.previous_nearest_range = battle.nearest_range;
  battle.nearest_range = INT16_MAX;
  for (int16_t k = 1; k < kShips; ++k)
    if ((ships[k].flags & kAfloat) && ships[k].range < battle.nearest_range) battle.nearest_range = ships[k].range;
  for (int16_t i = 0; i < kObjects; ++i)
    if (objects[i].state || objects[i].flags) battle.nearest_range = 0;
  if (!battle.entry_mode && battle.nearest_range > sight.previous_nearest_range && !ships[0].noise) {
    battle.range_opening += 1;
    if (battle.range_opening >= 2) {
      ask(text_range_increasing, text_continue_battle, 88, 60);
      answer_question([] { return battle.range_opening > 0; }, [] { battle.range_opening = -4; });
    }
  } else if (battle.range_opening > 0) {
    battle.range_opening = 0;
  }
}

// Long without a shot or a close contact: the crew asks whether to go on.
void check_quiet() {
  if (battle.entry_mode || battle.quiet_frames <= kQuietTimeout || battle.frame_count <= kQuietStart) return;
  audio::play_effect(kEffectSilenceAll);
  audio::engine_off();
  ask(text_no_contacts, text_do_we_continue, 120, 60);
  answer_question([] { return battle.quiet_frames > kQuietTimeout; }, [] { battle.quiet_frames = 0; });
}

// Whether the battle is over: no enemy ship afloat and no enemy weapon
// running (which also keeps the battle from being quiet), or the
// submarine lost.
bool battle_done() {
  bool done = true;
  for (int16_t k = 1; k < kShips; ++k)
    if (ships[k].flags & kAfloat) done = false;
  for (int16_t i = 0; i < kObjects; ++i) {
    if ((objects[i].state || objects[i].flags) && objects[i].kind == kEnemyWeapon) {
      done = false;
      battle.quiet_frames = 0;
    }
  }
  return done || (battle.damage_word & kDamageLost);
}

// After the battle: the replay offered until N (Alt-E: no; Alt-Q quits).
void offer_replay() {
  battle.scale = 6;
  battle.battle_over = 0;
  while (!battle.battle_over && !quit()) {
    audio::play_effect(kEffectSilenceAll);
    audio::engine_off();
    ask(text_mission_over, text_view_replay, 120, 50);
    int16_t key = 0;
    while (!battle.battle_over && !(key = input::getkey())) {
    }
    input::flush_keys();
    const int16_t ch = int16_t(key & 255);
    if (ch) {
      if (ch == 'n' || ch == 'N') battle.battle_over = 1;
      if (ch == 'y' || ch == 'Y') replay_log();
    } else if ((key >> 8) == kQuitKey) {
      ui::state().quit_key = 1;
    } else if ((key >> 8) == kEndKey) {
      battle.battle_over = 1;
    }
  }
}
}  // namespace

int16_t battle_main() {
  start_battle(*battle_start());
  uint32_t next_tick = 0;
  while (!battle.battle_over && !quit()) {
    if (battle.entry_mode) battle.typed_frames += 1;
    if (!battle.paused) {
      battle_step();
      battle.quiet_frames += 1;
    }
    sight.pending_effect = 0;
    redraw();
    engine_update();
    for (int k = 0; k < 2; ++k) {
      handle_keys();
      weapon_step();
    }
    if (battle.time_rate == kRealTime) {  // paced by the frames, more often
      while (next_tick > audio::frames()) screen::wait_tof();
      next_tick = audio::frames() + kTickFrames;
      for (int k = 0; k < 3; ++k) {
        handle_keys();
        weapon_step();
      }
    } else {  // accelerated time: paced as the A500 was by its speed (vars.hpp)
      while (next_tick > audio::frames()) screen::wait_tof();
      next_tick = audio::frames() + kAcceleratedTickFrames;
    }
    if (!battle.paused) battle.frame_count += 1;
    if (!(battle.frame_count & 63)) check_range_opening();
    check_quiet();
    if (battle.battle_over || quit()) continue;
    battle.battle_over = battle_done() ? 1 : 0;
  }
  offer_replay();
  if (!quit()) after_action_report();
  save_battle_state();
  input::flush_keys();
  screen::set_pointer(ui::state().pointer_8x8);
  screen::hide_pointer();
  return 0;
}
ORIGINAL_IN("storm", 0x20228, battle_main);

// $28ec0: the tubes page (2), or with `reloads` from it the reloads page
// (9); the page left is remembered unless it was one of these.
void tubes_page(int16_t reloads) {
  const int16_t page = battle.sub_mode;
  if (page != 2 && page != 9) battle.saved_sub_mode = page;
  battle.sub_mode = (page == 2 && reloads) ? int16_t(9) : int16_t(2);
  battle.redraw_left = 1;
}
ORIGINAL_IN("storm", 0x28ec0, tubes_page);

// $2d38c, $2d39c: the background colour red (a hit) or black.
void flash_red() { screen::colour(0, 0xe00); }
ORIGINAL_IN("storm", 0x2d38c, flash_red);
void flash_black() { screen::colour(0, 0); }
ORIGINAL_IN("storm", 0x2d39c, flash_black);

// $28ea0: display mode 3 with its scale (and a contact selected).
void enter_mode3b() {
  battle.display_mode = 3;
  battle.scale = battle.scales_by_mode[3];
  if (battle.selected_object == -1) select_next_contact();
  battle.redraw_setup = 1;
}
ORIGINAL_IN("storm", 0x28ea0, enter_mode3b);

// $25faa: ship i's bearing and range from the player, whether it was
// heard, and whether the player detects it (a score from its type, its
// speed and noise, and the conditions).
void track_ship(int16_t i) {
  Ship& s = ships[i];
  const Ship& own = ships[0];
  course_to(s.x, s.y, own.x, own.y);
  s.bearing = sight.course_bearing;
  s.range = sight.course_range;
  if (sight.course_range < kCloseRange) battle.contact_flags |= kShipClose;
  if (!(battle.frame_count & 24)) {
    s.flags = uint8_t((s.flags & ~kAlerted) | ((s.heard & 1) ? kAlerted : 0));
    s.heard &= uint8_t(~1);
  }
  const ShipType& t = ship_types[s.id];
  int16_t score = int16_t(t.noise + t.rating[0] + (s.speed >> 1));
  if (own.noise) score = int16_t(score + (int16_t(s.noise - own.noise) >> 3));
  score = int16_t(score + 2 * (battle.enemy_tactics & 3) + ((battle.frame_count >> 5) & 7));
  if (score > kDetectScore) s.flags |= kDetected;
}
ORIGINAL_IN("storm", 0x25faa, track_ship);

// $254bc: the ordered depth (10-999 feet; -1: none given).
void order_depth(int16_t v) {
  if (v == -1) return;
  ships[0].depth_set = std::clamp<int16_t>(v, 10, 999);
  battle.redraw_helm = 1;
}
ORIGINAL_IN("storm", 0x254bc, order_depth);

// $254ea: the ordered course in degrees (as a 16-bit angle, + 1 so that
// 0 means none); out of range: a message. The rudder is centred.
void order_course(int16_t deg) {
  if (deg == -1 || deg > 360) {
    status_message(text_course_range.str(), kPenWhite, 1);
    return;
  }
  const int16_t d = std::clamp<int16_t>(deg, 0, 359);
  ships[0].course = int16_t(uint16_t(uint16_t(int16_t(uint16_t(d << 6)) / 45) << 7) + 1);
  battle.rudder = 0;
  battle.redraw_helm = 1;
}
ORIGINAL_IN("storm", 0x254ea, order_course);

}  // namespace game::storm
