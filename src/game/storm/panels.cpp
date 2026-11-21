// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Storm (the battle module): the panels around the displays: labels,
// lists and the number entry's line.
#include "game/audio/calls.hpp"
#include "game/common/math.hpp"
#include "game/common/strings.hpp"
#include "game/input/calls.hpp"
#include "game/input/vars.hpp"
#include "game/mem/register.hpp"
#include "game/screen/calls.hpp"

#include <algorithm>
#include "game/storm/calls.hpp"
#include "game/storm/vars.hpp"
#include "game/ui/state.hpp"
#include "game/storm/texts.hpp"

#include <cstring>

namespace game::storm {

// $243b0, $28f04: the left panel's titles.
void threats_label() { text_shadow(text_threats.str(), 15, 104, 1); }
ORIGINAL_IN("storm", 0x243b0, threats_label);
void weapons_label() { text_shadow(text_weapons.str(), 15, 105, 1); }
ORIGINAL_IN("storm", 0x28f04, weapons_label);

// $2bbdc: the helm panel's units (the values are redrawn).
void helm_labels() {
  battle.helm_flag = 0;
  text_shadow(text_deg.str(), 38, 24, kMessageColour);
  text_shadow(text_knts.str(), 30, 38, kMessageColour);
  text_shadow(text_ft.str(), 40, 52, kMessageColour);
  battle.redraw_helm = 1;
  battle.redraw_own_ship = 1;
}
ORIGINAL_IN("storm", 0x2bbdc, helm_labels);

// $2bbb6: the lower left panel cleared.
void clear_lower_panel() { screen::fill_rect_colour(14, 104, 63, 181, 0); }
ORIGINAL_IN("storm", 0x2bbb6, clear_lower_panel);

// $2af34: the identification list: "A." to "Y." in two columns, each
// with the name of its ship type in pen 0 on pen 9.
void draw_letter_list() {
  constexpr int16_t kLeft = 88, kRight = 196, kNameOffset = 18;
  int16_t x = kLeft, y = 48;
  for (int16_t i = 0; i < kShipList; ++i) {
    texts().text_buffer[0] = char('A' + i);
    texts().text_buffer[1] = '.';
    texts().text_buffer[2] = 0;
    text_jam2(texts().text_buffer, x, y, kPenBlack, kPenWhite);
    const ShipType& t = ship_types[int16_t(ship_list[i])];
    text_jam2(const_text(type_names[t.name]), int16_t(x + kNameOffset), y, kPenBlack, kMessageColour);  // on pen 9
    if (x == kLeft) {
      x = kRight;
    } else {
      x = kLeft;
      y = int16_t(y + 8);
    }
  }
}
ORIGINAL_IN("storm", 0x2af34, draw_letter_list);

// $2d080: the number entry's line: its question centred, the digits typed
// so far after it.
void entry_line() {
  screen::fill_rect_colour(kStatusLeft, kStatusTop, kStatusRight, kStatusBottom, kPenBlack);
  const char* question = const_text(start_messages[int16_t(sight.question)]);
  text_shadow(question, int16_t(kStatusCentre - kStatusHalfChar * int16_t(std::strlen(question))), kStatusTextY, kPenLightGrey);
  text_shadow(texts().typed_digits, entry_x[battle.entry_mode], kStatusTextY, kPenLightGrey);
}
ORIGINAL_IN("storm", 0x2d080, entry_line);

// $2904e: the reloads left: up to five weapon types with their counts.
void reloads_list() {
  text_shadow(text_reloads.str(), 15, 105, kPenWhite);
  int16_t y = 112, shown = 0;
  for (int16_t w = 0; w < kWeaponTypes; ++w) {
    if (battle.weapon_counts[w] == 0 || shown >= kReloadLines) continue;
    text_shadow(const_text(weapon_names[w]), 15, y, kPenCrimson);
    y = int16_t(y + 7);
    text_shadow(common::format_number(battle.weapon_counts[w], 2), 27, y, kPenYellow);
    y = int16_t(y + 7);
    ++shown;
  }
}
ORIGINAL_IN("storm", 0x2904e, reloads_list);

// $28f22: the tubes: (with at most four) "TUBE n", then what is in it
// (dim when empty, bright when loaded); the TASM missiles left unless
// that system is damaged.
void tubes_list() {
  char* text = texts().text_buffer;
  int16_t y = 112;
  for (int16_t i = 0; i < battle.tube_count; ++i) {
    if (battle.tube_count <= 4) {
      string_copy(text, text_tube.str());
      string_append(text, common::format_number(int16_t(i + 1), 1));
      text_shadow(text, 15, y, kPenCrimson);
      y = int16_t(y + 7);
    }
    const int16_t tube = battle.tube_contents[i];
    string_copy(text, const_text(weapon_names[int16_t(tube & ~kTubeLoaded)]));
    string_append(text, text_pad.str());
    texts().text_buffer[kTubeNameLength] = 0;
    text_shadow(text, 15, y, tube == kPenBlack ? kPenLightBlue : (tube & kTubeLoaded) ? kPenYellow : kPenDarkRed);
    y = int16_t(y + 7);
  }
  if (!battle.weapon_counts[kTasm] || (battle.damage_word & kDamageTubes)) return;
  string_copy(text, common::format_number(battle.weapon_counts[kTasm], 2));
  string_append(text, text_tasm.str());
  y = int16_t(y + 7);
  text_shadow(text, 15, y, kPenYellow);
}
ORIGINAL_IN("storm", 0x28f22, tubes_list);

// $2bc30: the submarine's heading, speed and depth (with a mark while it
// cavitates and its depth code), drawn on the front bitmap over a copy of
// the panel and copied back.
void own_ship_panel() {
  const Ship& own = ships[0];
  screen::blit_bitmaps(*back_bitmap, 14, 15, *front_bitmap, 14, 15, 49, 42);
  screen::draw_to(*front_bitmap);
  text_shadow(common::format_degrees(own.heading), 15, 24, kMessageColour);
  text_shadow(common::format_number(own.speed, 2), 15, 38, kMessageColour);
  text_shadow(common::format_number(own.depth, 3), 15, 52, kMessageColour);
  text_shadow(((own.flags & kCavitating) ? text_cavitating : text_quiet).str(), 56, 38, kPenRed);
  below_limit(own.depth);
  text_shadow(texts().text_buffer, 52, 45, kMessageColour);
  screen::blit_bitmaps(*front_bitmap, 14, 15, *back_bitmap, 14, 15, 49, 42);
  screen::draw_to(*back_bitmap);
  battle.redraw_own_ship = 0;
}
ORIGINAL_IN("storm", 0x2bc30, own_ship_panel);

// $2b514: the damage report: "NONE", or the damaged systems (sonar,
// towed array, tubes, propeller linkage).
void damage_list() {
  text_shadow(text_damage.str(), 15, 105, kPenWhite);
  text_shadow(text_reports.str(), 15, 112, kPenWhite);
  text_shadow(text_dashes2.str(), 15, 119, kPenWhite);
  if (battle.damage_word == 0) {
    text_shadow(text_none.str(), 15, 133, kPenLightBlue);
    return;
  }
  struct System {
    uint8_t bit;
    const Const<char>& line1;
    const Const<char>& line2;
  };
  const System systems[] = {{kDamagePassiveSonar, text_passive, text_sonar},
                            {kDamageTowedArray, text_towed, text_array},
                            {kDamageTubes, text_torpedo, text_tubes},
                            {kDamagePropulsion, text_prop, text_linkage}};
  int16_t y = 126;
  for (const System& s : systems) {
    if (!(battle.damage_word & s.bit)) continue;
    text_shadow(s.line1.str(), 15, y, kPenRed);
    y = int16_t(y + 7);
    text_shadow(s.line2.str(), 15, y, kPenRed);
    y = int16_t(y + 7);
  }
}
ORIGINAL_IN("storm", 0x2b514, damage_list);

// $2bab0: the lower left panel for the page shown (outside page 4 and
// aiming, no weapon is active).
void left_panel() {
  clear_lower_panel();
  if (battle.display_mode != kDisplayPeriscope && battle.sub_mode != kPageTorpedo && !battle.aiming) battle.active_weapon = -1;
  switch (battle.sub_mode) {
    case kPageContacts: contact_label_panel(); break;
    case kPageWeapons: weapons_label(); break;
    case kPageDamage: damage_list(); break;
    case kPageTorpedo: torpedo_labels(); break;
    case kPageThreats: threats_label(); break;
    case kPageReloads: reloads_list(); break;
    default: break;
  }
  battle.redraw_left = 0;
}
ORIGINAL_IN("storm", 0x2bab0, left_panel);

// $2bb24: the lower right panel's values for the page shown, drawn on
// the front bitmap over a copy of the panel and copied back.
void right_panel() {
  screen::blit_bitmaps(*back_bitmap, 14, 104, *front_bitmap, 14, 104, 49, 77);
  screen::draw_to(*front_bitmap);
  switch (battle.sub_mode) {
    case kPageContacts: contact_panel(); break;
    case kPageWeapons: tubes_list(); break;
    case kPageTorpedo: weapon_panel(); break;
    case kPageThreats: torpedo_panel(); break;
    default: break;
  }
  screen::blit_bitmaps(*front_bitmap, 14, 104, *back_bitmap, 14, 104, 49, 77);
  screen::draw_to(*back_bitmap);
  battle.redraw_right = 0;
}
ORIGINAL_IN("storm", 0x2bb24, right_panel);

// $2b9d6: the current display's contents.
void draw_display() {
  switch (battle.display_mode) {
    case 1:
    case 3:
    case 5: tactical_display(); break;
    case kDisplayContact: contact_display(); break;
    case kDisplaySonar: sonar_update(); break;
    case kDisplayPeriscope: periscope_view(); break;
    case kDisplayLibrary: sea_noise(); break;
    case kDisplayIdentify: identify_update(); break;
    default: break;
  }
  battle.redraw_display = 0;
}
ORIGINAL_IN("storm", 0x2b9d6, draw_display);

// $2b654: the parts that changed, the display, and the right panel.
void redraw() {
  if (battle.redraw_own_ship) own_ship_panel();
  if (battle.redraw_helm) helm_panel();
  if (battle.redraw_setup) display_setup();
  draw_display();
  if (battle.redraw_left) left_panel();
  right_panel();
}
ORIGINAL_IN("storm", 0x2b654, redraw);

// $2aefc: display 8, the identification list.
void letters_display() {
  battle.display_mode = kDisplayIdentify;
  show_mode(battle.display_mode, 9);
  text_jam2(text_select_type.str(), 134, 34, kPenBlack, kPenWhite);
  draw_letter_list();
}
ORIGINAL_IN("storm", 0x2aefc, letters_display);

// $22bac: display 7, the sonar library (asking for a contact when none is
// selected).
void sonar_library_display() {
  if (battle.selected_ship == 0 || battle.selected_ship == -1) {
    status_message(text_select_contact.str(), kPenWhite, 1);
    status_if_flag();
  }
  battle.library_choice = -1;
  battle.display_mode = kDisplayLibrary;
  show_mode(kDisplayLibrary, 0);
  sonar_library();
}
ORIGINAL_IN("storm", 0x22bac, sonar_library_display);

// $2b686: display and page shown, everything to be redrawn (page 1 picks
// the next contact).
void set_display(int16_t display, int16_t page) {
  battle.shown_mode = -1;
  battle.display_mode = display;
  battle.redraw_setup = 1;
  battle.saved_sub_mode = -1;
  battle.sub_mode = page;
  battle.redraw_left = 1;
  if (page == 1) next_contact();
  helm_labels();
  battle.redraw_own_ship = 1;
  battle.redraw_helm = 1;
}
ORIGINAL_IN("storm", 0x2b686, set_display);

// $22408: display 4, the sonar conditions: a cut through the sea (the
// surface duct, the thermal layer and, by the sea bed, the bottom) with
// the transmission index, ambient noise and the layer's depth, and a
// depth scale every 100 feet.
void sonar_display() {
  const int16_t depth = battle.layer_depth;
  const int16_t mid = int16_t(kSonarSurface + depth / kFeetPerPixel + (depth % kFeetPerPixel >= 3 ? 1 : 0));
  const int16_t half = std::max<int16_t>(battle.layer_strength, 1);
  battle.display_mode = kDisplaySonar;
  show_mode(kDisplaySonar, 0);
  screen::fill_rect_colour(kMapLeft, kMapTop, kSonarRight, kSonarSurface - 1, kPenSky);
  screen::fill_rect_colour(kMapLeft, kSonarSurface + 1, kSonarRight, int16_t(mid - half), duct_colours[battle.duct_strength]);
  screen::fill_rect_colour(kMapLeft, int16_t(mid - half), kSonarRight, int16_t(mid + half), kPenDarkRed);
  screen::fill_rect_colour(kMapLeft, int16_t(mid + half + 1), kSonarRight, kMapBottom, 46);
  if (battle.depth_mode == 0) {
    const int16_t bottom = depth_at(ships[0].x, ships[0].y);
    if (bottom <= kDeepestBottom)
      screen::fill_rect_colour(kMapLeft, int16_t(kSonarSurface + bottom / kFeetPerPixel), kSonarRight, kMapBottom, 61);
  }
  screen::line_colour(kMapLeft, kSonarSurface, kSonarRight, kSonarSurface, kPenWhite);
  screen::box(kMapLeft, kMapTop, kSonarRight, kMapBottom, kPenWhite);

  char* buf = texts().text_buffer;
  constexpr int16_t kX = kMapLeft + 2, kColour = kPenYellow;
  int16_t y = 29;
  text(text_transmission.str(), kX, y, kColour);
  string_copy(buf, text_ambient.str());
  string_append(buf, common::format_number(int16_t(battle.ambient_noise * 3), 2));
  string_append(buf, text_av.str());
  y = int16_t(y + 8);
  text(buf, kX, y, kColour);
  y = kSonarSurface + 6;
  text(const_text(strength_names[battle.duct_strength]), kX, y, kColour);
  y = int16_t(y + 8);
  text(text_surface.str(), kX, y, kColour);
  y = int16_t(y + 8);
  text(text_duct.str(), kX, y, kColour);
  y = std::max<int16_t>(int16_t(y + 8), int16_t(mid + half + 8));
  text(const_text(strength_names[battle.layer_strength]), kX, y, kColour);
  y = int16_t(y + 8);
  text(text_thermal.str(), kX, y, kColour);
  y = int16_t(y + 8);
  text(text_layer.str(), kX, y, kColour);
  string_copy(buf, text_at.str());
  string_append(buf, common::format_number(battle.layer_depth, 3));
  y = int16_t(y + 8);
  text(buf, kX, y, kColour);
  y = int16_t(y + 8);
  text(text_feet.str(), kX, y, kColour);
  string_copy(buf, text_depth_mark.str());  // "-100-"
  y = kSonarSurface + 14;
  for (int16_t k = 0; k < 8; ++k) {
    text(buf, kSonarRight, y, kPenWhite);
    y = int16_t(y + 14);
    texts().text_buffer[1] = char('1' + k + 1);
  }
  screen::blit_bitmaps(*back_bitmap, kMapLeft, kMapTop, *front_bitmap, kMapLeft, kMapTop, 231, 141);
}
ORIGINAL_IN("storm", 0x22408, sonar_display);

// $22c02: the sonar library: "A" to "Y" in two columns, each with its
// ship type's sonar shades and name (11 characters); then the selected
// ship's shades.
void sonar_library() {
  constexpr int16_t kLeft = 80, kRight = 192, kNameLength = 11;
  char* text = texts().text_buffer;
  int16_t x = kLeft, y = 30;
  for (int16_t i = 0; i < kShipList; ++i) {
    texts().text_buffer[0] = char('A' + i);
    texts().text_buffer[1] = ' ';
    texts().text_buffer[2] = 0;
    text_shadow(text, x, y, kPenWhite);
    const int16_t type = int16_t(ship_list[i]);
    shade_column(i, type, int16_t(x + 8), y);
    string_copy(text, const_text(type_names[ship_types[type].name]));
    texts().text_buffer[kNameLength] = 0;
    text_shadow(text, int16_t(x + 42), y, kPenRed);
    if (x == kLeft) {
      x = kRight;
    } else {
      x = kLeft;
      y = int16_t(y + 8);
    }
  }
  sea_shades();
}
ORIGINAL_IN("storm", 0x22c02, sonar_library);

// $243ce: the threats panel: the enemy weapons that can be heard,
// nearest first (up to five): bearing (with the depth code) and range.
void torpedo_panel() {
  const Ship& own = ships[0];
  int16_t last = -1;
  for (int16_t i = 0; i < kObjects; ++i) {
    const Object& o = objects[i];
    if ((!o.state && !o.flags) || o.kind != kEnemyWeapon || o.noise <= kLoudWeapon) continue;
    course_to(own.x, own.y, o.x, o.y);
    int16_t k = 0;
    while (k <= last && sight.course_range >= battle.threats[k].range) ++k;
    for (int16_t j = last; j >= k; --j) battle.threats[j + 1] = battle.threats[j];
    battle.threats[k].object = i;
    battle.threats[k].range = sight.course_range;
    battle.threats[k].bearing = sight.course_bearing;
    ++last;
  }
  char* text = texts().text_buffer;
  int16_t y = 111;
  if (last == -1) {
    text_shadow(text_none2.str(), 15, y, kPenWhite);
    y = int16_t(y + 7);
  } else {
    for (int16_t k = 0; k <= std::min<int16_t>(last, kThreatLines - 1); ++k) {
      text_shadow(text_brg.str(), 15, y, kPenCrimson);
      const Object& o = objects[battle.threats[k].object];
      const int16_t colour = o.homing == 1 ? kPenYellow : kMessageColour;
      below_limit(o.depth);
      string_append(text, common::format_degrees(battle.threats[k].bearing));
      text_shadow(text, 33, y, colour);
      y = int16_t(y + 7);
      string_copy(text, text_rng.str());
      string_append(text, common::number_field(int32_t(battle.threats[k].range) * 4, 5));
      text_shadow(text, 15, y, colour);
      y = int16_t(y + 7);
    }
  }
  if (y < 181) screen::fill_rect_colour(14, y, 63, 181, kPenBlack);
}
ORIGINAL_IN("storm", 0x243ce, torpedo_panel);

// $2bd36: the helm panel: the rudder (or the ordered course), "LEVEL"
// (or the ordered depth), the A/T/R flags (active sonar, towed array, mast) and
// the submarine's noise; drawn over a copy of the panel and copied back.
void helm_panel() {
  const Ship& own = ships[0];
  char* text = texts().text_buffer;
  screen::blit_bitmaps(*back_bitmap, 14, 60, *front_bitmap, 14, 60, 49, 40);
  screen::draw_to(*front_bitmap);
  if (own.course == 0) {
    const int16_t r = battle.rudder;
    string_copy(text, (r < 0 ? text_rudder_left : r > 0 ? text_rudder_right : text_steady).addr());
    if (r != 0) string_append(text, common::format_number(common::abs_saturated(int16_t(r * 5)), 2));
  } else {
    string_copy(text, text_to_course.str());
    string_copy(text + kCourseDigits, common::format_degrees(own.course));
    texts().text_buffer[kCourseDigits + 3] = ' ';
  }
  text_shadow(text, 15, 68, kMessageColour);
  if (own.depth_set) {
    string_copy(text, text_to.str());
    string_append(text, common::format_number(own.depth_set, 3));
  } else {
    string_copy(text, text_level.str());
  }
  text_shadow(text, 15, 81, kMessageColour);
  const bool towed = (own.flags & kTowedArray) && !(battle.damage_word & kDamageTowedArray);
  text_shadow(((own.flags & kDetected) ? text_flag_a : text_flag_a_off).str(), 17, 91, kMessageColour);
  text_shadow((towed ? text_flag_t : text_flag_t_off).str(), 25, 91, kMessageColour);
  text_shadow(((own.flags & kMastRaised) ? text_flag_r : text_flag_r_off).str(), 33, 91, kMessageColour);
  const int16_t noise = int16_t(ship_types[own.id].noise * 2 + ((own.flags & kNoisy) ? 8 : 0) + own.speed +
                                ((own.flags & kCavitating) ? 4 : 0) - 6);
  text_shadow(common::format_number(noise, 3), 41, 91, kMessageColour);
  screen::blit_bitmaps(*front_bitmap, 14, 60, *back_bitmap, 14, 60, 49, 40);
  screen::draw_to(*back_bitmap);
  battle.redraw_helm = 0;
}
ORIGINAL_IN("storm", 0x2bd36, helm_panel);

ShadeLevels::ShadeLevels(const ShipType& t, int16_t phase)
    : level_(t.kind == kSubmarineKind ? int16_t(0) : int16_t(16)),
      step_(int16_t(t.toughness / 16 + 2)),
      mask_(int16_t((-(t.noise + 4)) / 2 & 15)),
      phase_(phase) {}

// Line i: the level grows by the step, less a pattern by the phase; 0-3
// give the pens 0, 16, 24, 12.
int16_t ShadeLevels::next_pen(int16_t i) {
  static constexpr int16_t kPens[] = {0, 16, 24, 12};
  level_ = int16_t(level_ + step_);
  const int16_t pattern = int16_t(((i + 7 * phase_) & mask_) << 5);
  return kPens[std::clamp<int16_t>(int16_t((level_ - pattern) / 32), 0, 3)];
}

// $22cc8: a ship type's sonar shades as 32 short lines from (x, y).
void shade_column(int16_t phase, int16_t type, int16_t x, int16_t y) {
  ShadeLevels shades(ship_types[type], phase);
  for (int16_t i = 0; i < kShadeLines; ++i) {
    screen::set_apen(shades.next_pen(i));
    screen::draw_line(int16_t(x + i), y, int16_t(x + i), int16_t(y + 4));
  }
}
ORIGINAL_IN("storm", 0x22cc8, shade_column);

// $2bfd8: the tactical display's grid, 512 >> scale pixels apart from the
// centre. Displays other than 3 and 5 take the pen from a local the
// original never sets; it is 6 in every scenario (the battle log's map).
void draw_grid() {
  constexpr int16_t kCentreX = 188;
  const int16_t pen = battle.display_mode == 3 ? kGridPen3 : battle.display_mode == 5 ? kGridPen5 : kGridPenOther;
  const int16_t step = int16_t(512 >> battle.scale);
  for (int16_t d = 0; kCentreX + d < kMapRight; d = int16_t(d + step)) {
    screen::line_colour(int16_t(kCentreX + d), kMapTop, int16_t(kCentreX + d), kMapBottom, pen);
    screen::line_colour(int16_t(kCentreX - d), kMapTop, int16_t(kCentreX - d), kMapBottom, pen);
  }
  for (int16_t d = 0; kMapCentreY + d < kMapBottom; d = int16_t(d + step)) {
    screen::line_colour(kMapLeft, int16_t(kMapCentreY + d), kMapRight, int16_t(kMapCentreY + d), pen);
    screen::line_colour(kMapLeft, int16_t(kMapCentreY - d), kMapRight, int16_t(kMapCentreY - d), pen);
  }
}
ORIGINAL_IN("storm", 0x2bfd8, draw_grid);

// $2bf8a: the tactical display's background (the sea, then the scale's
// ticks on display 1 or the grid) on the front bitmap.
void tactical_background() {
  screen::draw_to(*front_bitmap);
  screen::fill_rect_colour(kMapLeft, kMapTop, kMapRight, kMapBottom, battle.sea_colour);
  if (battle.display_mode == 1) draw_ticks();
  else draw_grid();
  screen::draw_to(*back_bitmap);
}
ORIGINAL_IN("storm", 0x2bf8a, tactical_background);

// $291fa: the torpedo panel's labels.
void torpedo_labels() {
  struct Label {
    const Const<char>& text;
    int16_t x, y, colour;
  };
  const Label labels[] = {
      {text_torp, 15, 105, 1},     {text_dashes, 15, 112, 1}, {text_time_to, 15, 119, 3},
      {text_run, 15, 126, 3},      {text_time_to2, 15, 133, 3}, {text_pap, 15, 140, 3},
      {text_speed2, 15, 147, 3},   {text_kts, 39, 154, 7},    {text_heading, 15, 161, 3},
      {text_deg2, 39, 168, 7},     {text_search, 21, 175, 3},
  };
  for (const Label& l : labels) text_shadow(l.text.str(), l.x, l.y, l.colour);
}
ORIGINAL_IN("storm", 0x291fa, torpedo_labels);

// $2dda2: the whole battle screen drawn again (after an event picture).
void redraw_screen() {
  screen::fill_rect_colour(0, 0, 319, 199, kPenBlack);
  screen::set_palette(battle_palette.array(screen::kPaletteWords));
  fresh_screen();
  set_display(battle.display_mode, battle.sub_mode);
  display_setup();
  left_panel();
  draw_display();
  right_panel();
  input::flush_keys();
  if (battle.entry_mode) entry_line();
}
ORIGINAL_IN("storm", 0x2dda2, redraw_screen);

// $2dd4c: (with event pictures on) the picture sequence of an event, in
// silence, then the screen redrawn. A ship's final hit has its own
// sequence.
void show_event(int16_t what, int16_t unused, int16_t cls, int16_t final) {
  (void)unused;
  if (!battle.action_track) return;
  if (final) {
    if (what == kPictureShipHitMissile) what = kPictureShipSunkMissile;
    else if (what == kPictureShipHitTorpedo) what = kPictureShipSunkTorpedo;
  }
  audio::play_effect(kEffectSilenceAll);
  audio::engine_off();
  screen::play_sequence(what, cls);
  audio::play_effect(kEffectSilenceAll);
  redraw_screen();
}
ORIGINAL_IN("storm", 0x2dd4c, show_event);

// $28b50: an RBU (rocket depth charge) attack on the submarine: damage,
// its picture (sunk: the final one), the events.
void rbu_attack() {
  status_message(text_rbu_attack.str(), kPenRed, 1);
  random_damage();
  const Ship& own = ships[0];
  if (own.events & kEventSunk) {
    show_event(kPictureSunkByRbu, 2, int16_t(own.id), 1);
    battle.damage_word |= kDamageSunkShown;
  } else {
    show_event(kPictureRbuAttack, 2, int16_t(own.id), 0);
  }
  report_events();
}
ORIGINAL_IN("storm", 0x28b50, rbu_attack);

// $29172: page 4, the torpedoes fired: the next one selected (none: a
// message); a running one becomes the active weapon with its aim point.
void weapon_page() {
  const int16_t previous = battle.selected_object;
  select_next_contact();
  if (battle.selected_object == -1) {
    status_message(text_no_torpedos.str(), kPenWhite, 1);
    status_if_flag();
    return;
  }
  const int16_t w = battle.selected_object;
  if (objects[w].flags & kWired) {
    battle.active_weapon = w;
    battle.wire_aimed = 0;
    load_aim(w);
  } else {
    battle.active_weapon = -1;
  }
  if (battle.display_mode == 3 && battle.selected_object != previous) battle.redraw_setup = 1;
  battle.saved_sub_mode = battle.sub_mode;
  battle.sub_mode = kPageTorpedo;
  battle.redraw_left = 1;
}
ORIGINAL_IN("storm", 0x29172, weapon_page);

// $21e5c: display 6, the periscope: its frame, the bearing (the selected
// contact's, else ahead); too deep, only the frame and a message. Raising
// it ends any aiming.
void periscope_display() {
  sight.periscope_phase = 1;
  battle.display_mode = kDisplayPeriscope;
  show_mode(kDisplayPeriscope, 0);
  screen::filled_box(120, 83, 257, 146, 62);
  screen::filled_box(121, 84, 256, 145, 62);
  screen::filled_box(123, 86, 254, 143, kPenLightGrey);
  const Ship& own = ships[0];
  sight.periscope_bearing = battle.selected_ship != -1 ? int16_t(ships[battle.selected_ship].bearing + kHalfTurn) : own.heading;
  if (own.depth > kPeriscopeDepth - 1) {
    status_message(text_too_deep.str(), kPenWhite, 1);
    status_if_flag();
    screen::fill_rect_colour(124, 87, 253, 142, kSeaPen);
    screen::draw_image(kPeriscopeImage, 177, 99);
    random_sea();
    return;
  }
  if (battle.aiming) {
    battle.aiming = 0;
    battle.active_weapon = -1;
    clear_status();
  }
  screen::draw_to(*front_bitmap);
  screen::fill_rect_colour(124, 87, 253, 115, kSkyPen);
  screen::fill_rect_colour(124, 116, 253, 142, kSeaPen);
  screen::draw_image(kPeriscopeImage, 177, 99);
  screen::draw_to(*back_bitmap);
  screen::blit_bitmaps(*front_bitmap, 124, 87, *back_bitmap, 124, 87, 130, 56);
  text_shadow(text_bearing_label.str(), 152, 150, kPenPaleBlue);
  text_shadow(common::format_degrees(sight.periscope_bearing), 206, 150, kPenPaleBlue);
  random_sea();
}
ORIGINAL_IN("storm", 0x21e5c, periscope_display);

// $22020: the periscope's view this frame (turned by the steering keys):
// the sky, the enemy ships on the surface within its field (by bearing,
// their image by aspect, lower as they are further), the waves and the
// reticle. A ship under the reticle is seen: identified, loud, its range
// shown. Too deep: only the sea.
void periscope_view() {
  constexpr int16_t kViewLeft = 124, kTop = 87, kViewRight = 253, kHorizon = 115, kBottom = 142, kTextY = 158;
  const Ship& own = ships[0];
  if (own.depth > kPeriscopeDepth - 1) {
    screen::draw_to(*front_bitmap);
    screen::fill_rect_colour(kViewLeft, kTop, kViewRight, kBottom, kSeaPen);
    screen::draw_image(kPeriscopeImage, 177, 99);
    screen::draw_to(*back_bitmap);
    screen::blit_bitmaps(*front_bitmap, kViewLeft, kTop, *back_bitmap, kViewLeft, kTop, 130, 56);
    screen::fill_rect_colour(kViewLeft, 150, kViewRight, kMapBottom, kPenBlack);
    return;
  }
  int16_t reticle = kPeriscopeImage, lift = 0;
  int16_t range = 0;
  sight.periscope_phase ^= 1;
  sight.seen_type = -1;
  if (uint8_t(battle.steer_keys) & input::kRight) {
    sight.periscope_bearing += 256;
    sight.wave_x -= 8;
  } else if (uint8_t(battle.steer_keys) & input::kLeft) {
    sight.periscope_bearing -= 256;
    sight.wave_x += 8;
  }
  text_shadow(text_bearing_label2.str(), 152, 150, kPenPaleBlue);
  text_shadow(common::format_degrees(sight.periscope_bearing), 206, 150, kPenPaleBlue);
  screen::draw_to(*front_bitmap);
  screen::fill_rect_colour(kViewLeft, kTop, kViewRight, kHorizon, kSkyPen);
  for (int16_t i = 1; i < kShips; ++i) {
    Ship& s = ships[i];
    if (!(s.flags & kAfloat) || s.side != 1 || s.depth != 0) continue;
    course_to(own.x, own.y, s.x, s.y);
    const int16_t place = int16_t(64 + (int16_t(sight.course_bearing - sight.periscope_bearing) >> 4));
    if (place < 0 || place >= 128) continue;
    const int16_t aspect =
        int16_t((uint16_t((s.heading & ~255) - (sight.periscope_bearing & ~255) + kHalfOctant) >> 13) & 7);
    lift = int16_t(int16_t((sight.course_range >> 7) - (60 - own.depth)) >> 4);
    if (!sight.periscope_phase) sight.ship_image = aspect;
    screen::draw_image(sight.ship_image, int16_t(place + 114), int16_t(lift + 111));
    if (place < 40 || place >= 88) continue;
    range = sight.course_range;
    sight.seen_type = int16_t(s.id);
    reticle = kReticleImage;
    s.noise = 255;
    s.flags_7 |= (kIdentified | kPlayerContact);
  }
  screen::fill_rect_colour(kViewLeft, kHorizon + 1, kViewRight, kBottom, kSeaPen);
  if (sight.sea_type == kFirstWaveImage) random_conditions();
  if (sight.sea_type == kLastWaveImage) random_sea();
  const int16_t wave = sight.sea_type;
  sight.sea_type += 1;
  screen::draw_image(wave, int16_t(156 + sight.wave_x), int16_t(lift + sight.wave_y + 122));
  screen::draw_image(reticle, 177, 99);
  screen::draw_to(*back_bitmap);
  screen::blit_bitmaps(*front_bitmap, kViewLeft, kTop, *back_bitmap, kViewLeft, kTop, 130, 56);
  if (sight.seen_type != -1) {
    text_shadow(text_range_label.str(), 152, kTextY, kPenRed);
    text_shadow(common::number_field(int32_t(range) * 4, 5), 190, kTextY, kPenRed);
  } else {
    screen::fill_rect_colour(152, kTextY, 240, 166, kPenBlack);
  }
}
ORIGINAL_IN("storm", 0x22020, periscope_view);

// $2953e: the selected torpedo's figures on page 4: its number, the time
// to run and to the point it seeks from, speed, heading, depth code, the
// wire (W/X) and turn (R/L) marks, and once seeking, whether it homes.
// Gone: the panel is blanked and nothing is selected.
void weapon_panel() {
  const int16_t sel = battle.selected_object;
  if (sel < 0) return;
  const Object& o = objects[sel];
  if (!o.state && !o.flags) {
    battle.selected_object = -1;
    for (int16_t y = 110; y < 180; y = int16_t(y + 7))
      for (int16_t x = 15; x < 63; x = int16_t(x + 6)) draw_char(21, x, y, 30, 0);
    return;
  }
  text_shadow(common::format_number(int16_t(sel + 1), 1), 51, 105, kPenWhite);
  const int16_t to_seek = common::clamp(int16_t(o.time - o.seek_time), 0, kMaxRunTime);
  text_shadow(common::format_number(common::clamp(int16_t((o.time + to_seek) >> 1), 0, kMaxRunTime), 3), 45, 126, kPenYellow);
  text_shadow(common::format_number(common::clamp(to_seek, 0, kMaxRunTime), 3), 45, 140, kPenYellow);
  text_shadow(common::format_number(o.speed, 3), 15, 154, kPenYellow);
  text_shadow(common::format_degrees(o.heading), 15, 168, kPenYellow);
  below_limit(o.depth);
  text_shadow(texts().text_buffer, 57, 175, kPenYellow);
  text_shadow(((o.flags & kWired) ? text_wire : text_no_wire).str(), 57, 147, kPenYellow);
  text_shadow(((o.flags & kTurnsRight) ? text_turn_right : text_turn_left).str(), 15, 175, kPenYellow);
  if (o.seek_time <= o.time) return;
  const Const<char>& state = o.homing > 0 ? text_homing : o.homing < 0 ? text_jammed : text_active;
  text_shadow(state.str(), 15, 112, o.homing > kPenBlack ? kPenYellow : kPenLightBlue);
}
ORIGINAL_IN("storm", 0x2953e, weapon_panel);

// $2ce2c: the tactical analysis for the most pressing situation (if it
// has a message): its lines with the value filled in, until a key.
void tactical_analysis() {
  const ConstPtr<uint8_t> message{analysis_texts[battle.highest_b]};
  const int16_t value = battle.highest_b_value;
  if (!message) return;
  const int16_t width = value > 9 ? int16_t(2) : int16_t(1);
  screen::hide_pointer();
  show_mode(battle.display_mode, 12);
  text_jam2(text_tactical_analysis.str(), 100, 40, kPenBlack, kPenWhite);
  char* line = texts().text_buffer;
  int16_t at = 0, y = 56;
  for (int32_t i = 0;; ++i) {
    const uint8_t c = message[i];
    if (c == kCodeEnd) break;
    if (c == 0) {
      texts().text_buffer[at] = 0;
      text_jam2(line, 100, y, kPenBlack, kPenCyan);
      y = int16_t(y + 8);
      at = 0;
    } else if (c == kCodeDegrees) {
      string_copy(line + uint32_t(at), common::format_degrees(value));
      at = int16_t(at + 3);
      texts().text_buffer[at++] = ' ';
    } else if (c == kCodeNumber) {
      string_copy(line + uint32_t(at), common::format_number(value, width));
      at = int16_t(at + width);
      texts().text_buffer[at++] = ' ';
    } else {
      texts().text_buffer[at++] = char(c);
    }
  }
  text_jam2(text_press_any_key.str(), 100, 150, kPenBlack, kPenWhite);
  while (!input::getkey()) {
  }
  input::flush_keys();
  const int16_t m = battle.display_mode;
  if (m != 3 && m != 5 && m != 1) battle.redraw_setup = 1;
}
ORIGINAL_IN("storm", 0x2ce2c, tactical_analysis);

// $2b6cc: a display set up after it or its scale changed. Tactical
// displays: the title, the centre (a selected torpedo on display 3, else
// the submarine, on display 1 moved towards the nearest contact when
// zoomed in), the background or depth chart, and the aiming line. Others:
// their own setup.
void display_setup() {
  const int16_t m = battle.display_mode;
  if (m != 1 && m != 3 && m != 5) {
    show_mode(battle.display_mode, 0);
    switch (battle.display_mode) {
      case kDisplayContact: sonar_analysis_display(); break;
      case kDisplaySonar: sonar_display(); break;
      case kDisplayPeriscope: periscope_display(); break;
      case kDisplayLibrary: sonar_library_display(); break;
      case kDisplayIdentify: letters_display(); break;
      default: break;
    }
    battle.redraw_setup = 0;
    return;
  }
  battle.sea_colour = 0;
  show_mode(battle.display_mode, 0);
  const int16_t sel = battle.selected_object;
  int16_t nearest = -1, best = INT16_MAX;
  for (int16_t i = 0; i < kShips; ++i) {
    const Ship& s = ships[i];
    if (s.side != 1 || !(s.flags & kAfloat) || !(s.flags_7 & kPlayerContact)) continue;
    if (s.range >= best || s.depth < 0) continue;
    best = s.range;
    nearest = i;
  }
  if (battle.display_mode == 3 && sel >= 0 && (objects[sel].weapon == kMark48 || objects[sel].weapon == kSeaLance)) {
    battle.selected_object -= 1;
    weapon_page();
    battle.sub_mode = kPageTorpedo;
    battle.redraw_left = 1;
    battle.view_x = objects[sel].x;
    battle.view_y = objects[sel].y;
  } else {
    battle.view_x = ships[0].x;
    battle.view_y = ships[0].y;
    if (battle.display_mode == 1 && nearest != -1 && battle.scale > 3) {
      int16_t size = int16_t(80 << battle.scale);
      if (best < size) size = best;
      const int16_t towards = int16_t(ships[nearest].bearing + kHalfTurn);
      battle.view_x += int16_t(common::sin_mul(towards, size) >> 1);
      battle.view_y -= int16_t(common::cos_mul(towards, size) >> 1);
    }
  }
  for (int16_t i = 0; i < kShips; ++i) ships[i].mark_x = 0;
  for (int16_t i = 0; i < kObjects; ++i) objects[i].mark_x = 0;
  if (battle.depth_chart_on == 1) {
    depth_chart();
    screen::blit_bitmaps(*back_bitmap, kMapLeft, kMapTop, *front_bitmap, kMapLeft, kMapTop, 231, 141);
  } else {
    tactical_background();
  }
  if (battle.active_weapon == -1) {
    battle.redraw_setup = 0;
    return;
  }
  aim_to_screen();
  if (battle.aiming) {
    screen::fill_rect_colour(kStatusLeft, kStatusTop, kStatusRight, kStatusBottom, kPenBlack);
    text_shadow(text_fire2.str(), 75, kStatusTextY, kPenRed);
    text_shadow(const_text(weapon_names[battle.fired_weapon]), 104, kStatusTextY, kPenRed);
    text_shadow(text_bearing_word2.str(), 160, kStatusTextY, kPenRed);
    text_shadow(text_range_word2.str(), 234, kStatusTextY, kPenRed);
    texts().last_status[0] = 0;
  }
  battle.redraw_setup = 0;
}
ORIGINAL_IN("storm", 0x2b6cc, display_setup);

// $2ab30: display 2, the selected contact: its name (its type once fully
// identified, else its class's first name) and gauges of the player's
// sensors on it and, identified, its own.
void contact_display() {
  const int16_t n = battle.selected_ship;
  const Ship& s = ships[n];
  const Ship& own = ships[0];
  if (!(s.flags_7 & kPlayerContact)) return;
  const bool known = (s.flags_7 & 3) == 3;
  screen::draw_to(*front_bitmap);
  if (battle.selected_ship != battle.named_ship) {
    if (known) battle.named_ship = battle.selected_ship;
    string_copy(texts().text_buffer, known ? const_text(type_names[ship_types[s.id].name]) : const_text(class_names[0]));
    screen::fill_rect_colour(158, 43, 248, 48, kPenBlack);
    text_shadow(texts().text_buffer, 158, 43, kPenOrange);
    screen::fill_rect_colour(76, 125, 123, 130, kPenBlack);
    text_shadow(texts().text_buffer, 76, 125, kPenOrange);
    screen::blit_bitmaps(*front_bitmap, 158, 43, *back_bitmap, 158, 43, 90, 6);
    screen::blit_bitmaps(*front_bitmap, 76, 125, *back_bitmap, 76, 125, 90, 6);
  }
  gauge(s.gauge_active, 73, (own.flags & kDetected) ? int16_t(7) : int16_t(30));
  gauge(s.gauge_hull, 81, (battle.damage_word & kDamagePassiveSonar) ? int16_t(30) : int16_t(7));
  gauge(s.gauge_towed, 89, (!(battle.damage_word & kDamageTowedArray) && (own.flags & kTowedArray)) ? int16_t(7) : int16_t(30));
  gauge(known ? int16_t(s.gauge_46) : kEmptyGauge, 109, s.hears_by == 0 ? int16_t(24) : int16_t(30));
  gauge(known ? int16_t(s.gauge_48) : kEmptyGauge, 117, 24);
  gauge(known ? int16_t(s.gauge_50) : kEmptyGauge, 125, (s.flags & kTowedArray) ? int16_t(24) : int16_t(30));
  screen::draw_to(*back_bitmap);
  screen::blit_bitmaps(*front_bitmap, 125, 97, *back_bitmap, 125, 97, 178, 34);
  screen::blit_bitmaps(*front_bitmap, 125, 133, *back_bitmap, 125, 133, 178, 34);
}
ORIGINAL_IN("storm", 0x2ab30, contact_display);

// $2a818: display 2's frame, the sonar detection analysis of the selected
// contact (page 1 only): the gauges' bands and labels and the player's
// type.
void sonar_analysis_display() {
  if (battle.selected_ship == 0 || battle.sub_mode != kPageContacts) {
    status_message(text_select_contact2.str(), kPenWhite, 1);
    status_if_flag();
    return;
  }
  battle.display_mode = kDisplayContact;
  show_mode(kDisplayContact, 0);
  battle.named_ship = -1;
  screen::line_colour(kMapLeft, 52, kMapRight, 52, kPenIndigo);
  text_shadow(text_sonar_analysis.str(), 80, 36, kPenWhite);
  text_shadow(text_target_class.str(), 80, 43, kPenWhite);
  struct Band {
    int16_t x0, y0, x1, y1, pen;
  };
  static constexpr Band kBands[] = {
      {125, 84, 199, 96, 16},  {125, 97, 199, 120, 48},  {125, 121, 199, 132, 16}, {125, 133, 199, 156, 48},
      {200, 69, 225, 96, 5},   {200, 97, 225, 120, 37},  {200, 121, 225, 132, 5},  {200, 133, 225, 156, 37},
      {226, 84, 301, 96, 21},  {226, 97, 301, 120, 53},  {226, 121, 301, 132, 21}, {226, 133, 301, 156, 53},
  };
  for (const Band& b : kBands) screen::fill_rect_colour(b.x0, b.y0, b.x1, b.y1, b.pen);
  struct Label {
    const Const<char>& text;
    int16_t x, y, colour;
  };
  const Label labels[] = {
      {text_undetectable, 127, 78, 16}, {text_tracking_range, 198, 63, 5}, {text_detection, 228, 72, 21},
      {text_threshold, 228, 78, 21},    {text_g_active, 76, 99, 7},        {text_g_passive, 76, 107, 7},
      {text_g_towed, 76, 115, 7},       {text_g_active2, 76, 135, 24},     {text_g_passive2, 76, 143, 24},
      {text_g_towed2, 76, 151, 24},
  };
  for (const Label& l : labels) text_shadow(l.text.str(), l.x, l.y, l.colour);
  text_shadow(const_text(type_names[ship_types[ships[0].id].name]), 76, 89, kPenOrange);
  screen::blit_bitmaps(*back_bitmap, 120, 97, *front_bitmap, 120, 97, 184, 77);
}
ORIGINAL_IN("storm", 0x2a818, sonar_analysis_display);

// $2c776: the after action report: each enemy and what became of it,
// until a key or fire.
void after_action_report() {
  input::flush_keys();
  screen::draw_to(*back_bitmap);
  screen::fill_rect_colour(0, 0, 319, 199, kPenBlack);
  screen::blit_bitmaps(*back_bitmap, 0, 0, *front_bitmap, 0, 0, 319, 199);
  screen::show_bitmap(*back_bitmap);
  text_style->font = 2;
  text(text_to_comsublant.str(), 16, 16, kPenPaleBlue);
  text(text_re_report.str(), 19, 32, kPenPaleBlue);
  text(text_enemy_engaged.str(), 16, 64, kPenCyan);
  text(text_result.str(), 168, 64, kPenCyan);
  int16_t y = 80;
  for (int16_t i = 1; i < kShips; ++i) {
    const Ship& s = ships[i];
    if (s.side != 1) continue;
    text(const_text(type_names[ship_types[s.id].name]), 16, y, kPenRed);
    const bool afloat = s.flags & kAfloat, damaged = s.flags & kDamaged;
    if (afloat && !damaged) text((s.id == 0 ? text_departed : text_escaped).str(), 168, y, kPenPurple);
    if (afloat && damaged) text(text_damaged.str(), 168, y, kPenPurple);
    if (!afloat) {
      if (s.id != 0) text(text_sunk.str(), 168, y, kPenPurple);
      else text((battle.aircraft_shot_down ? text_shot_down : text_departed2).str(), 168, y, kPenPurple);
    }
    y = int16_t(y + 12);
  }
  text(text_press_continue.str(), 60, 192, kPenYellow);
  bool done = false;
  while (!done) {
    if (input::getkey() || (input::controls() & input::kFire)) done = true;
  }
  screen::fill_rect_colour(0, 0, 319, 199, kPenBlack);
  text_style->font = 1;
  input::flush_keys();
}
ORIGINAL_IN("storm", 0x2c776, after_action_report);

namespace {
// The sea conditions picture's row for a depth (7 feet a pixel, rounded),
// at most the bottom row.
int16_t sonar_row(int16_t depth) {
  return int16_t(kSonarSurface + depth / kFeetPerPixel + (depth % kFeetPerPixel >= 3 ? 1 : 0) - 3);
}

// A ship type's name (8 characters) or its class's, in text_buffer.
void short_name(const char* name) {
  std::strcpy(texts().text_buffer, name);
  texts().text_buffer[kTubeNameLength] = 0;
}
}  // namespace

// $22766: (on even frames) the sea conditions picture: the sea bed's
// level, the submarine and the identified contacts at their depths (from
// the right: surface ships and submarines in their own columns) with
// their class or type names. Drawn on bitmap 2 from the front bitmap and
// copied to the back one.
void sonar_update() {
  constexpr int16_t kPictureBitmap = 2, kOwnX = 77, kColumn = 42, kBottomRow = 156;
  constexpr int16_t kSubImage = 10, kShipImage = 11, kEnemySubImage = 12;
  if (battle.frame_count & 1) return;
  int16_t ship_x = 229, sub_x = 229;
  screen::blit_bitmaps(*front_bitmap, kMapLeft, kMapTop, kPictureBitmap, kMapLeft, kMapTop, 231, 141);
  screen::draw_to(kPictureBitmap);
  const Ship& own = ships[0];
  text_on_bitmap2(common::format_number(int16_t((area_level(own.x, own.y) + 3) * 2), 2), 189, 29, kPenYellow);
  const int16_t own_row = std::min<int16_t>(sonar_row(own.depth), kBottomRow);
  screen::draw_image(kSubImage, kOwnX, own_row);
  short_name(const_text(type_names[ship_types[own.id].name]));
  text_on_bitmap2(texts().text_buffer, kOwnX, int16_t(own_row + 6), kPenWhite);
  for (int16_t i = 1; i < kShips; ++i) {
    const Ship& s = ships[i];
    if (!(s.flags & kAfloat) || s.side != 1) continue;
    const ShipType& t = ship_types[s.id];
    const bool shown = (t.kind == kSurfaceShipKind && (s.flags_7 & 0x01)) || (t.kind == kSubmarineKind && (s.flags_7 & 0x09) >= 9);
    if (!shown) continue;
    int16_t row = int16_t((sonar_row(s.depth) >> 2) << 2);
    if (s.depth == 0) row = kSonarSurface - 5;
    row = std::min<int16_t>(row, kBottomRow);
    if (t.kind == kSubmarineKind) {
      screen::draw_image(kShipImage, ship_x, row);
      const int16_t known = int16_t(s.flags_7 & (kIdentified | kPlayerContact));
      if (known == 9 || known == 10 || known == 11) {
        short_name(known == 11 ? const_text(type_names[t.name]) : const_text(class_names[t.kind]));
        text_on_bitmap2(texts().text_buffer, ship_x, int16_t(row + 6), kPenCrimson);
      }
      ship_x = int16_t(ship_x - kColumn);
    } else if (t.kind == kSurfaceShipKind) {
      screen::draw_image(kEnemySubImage, sub_x, row);
      const int16_t known = int16_t(s.flags_7 & 3);
      if (known != 0) {
        short_name(known == 3 ? const_text(type_names[t.name]) : const_text(class_names[t.kind]));
        text_on_bitmap2(texts().text_buffer, sub_x, int16_t(row + 6), kPenCrimson);
      }
      sub_x = int16_t(sub_x - kColumn);
    }
  }
  screen::draw_to(*back_bitmap);
  screen::blit_bitmaps(kPictureBitmap, kMapLeft, kMapTop, *back_bitmap, kMapLeft, kMapTop, 231, 141);
}
ORIGINAL_IN("storm", 0x22766, sonar_update);

// $21808: the contact page's labels: the selected ship's type once fully
// identified, and the empty fields; "NONE" (and ship 0 selected) without
// a contact. On the periscope the display is set up again.
void contact_label_panel() {
  text_shadow(text_contact_label.str(), 15, 105, kPenWhite);
  if (battle.selected_ship == -1) {
    battle.selected_ship = 0;
    text_shadow(text_none3.str(), 15, 105, kPenIndigo);
    return;
  }
  const Ship& s = ships[battle.selected_ship];
  if ((s.flags_7 & 3) == 3) {
    short_name(const_text(type_names[ship_types[s.id].name]));
    text_shadow(texts().text_buffer, 15, 112, kPenYellow);
  }
  struct Label {
    const Const<char>& text;
    int16_t x, y, colour;
  };
  const Label labels[] = {
      {text_c_bearing, 15, 119, 3}, {text_c_deg, 15, 126, 7},         {text_sig_snsr, 15, 133, 3},
      {text_sol, 15, 147, 3},       {text_percent, 53, 147, 7},       {text_crs_spd, 15, 154, 3},
      {text_crs_spd_none, 15, 161, 7}, {text_c_range, 15, 168, 3},    {text_kyds_none, 15, 175, 7},
  };
  for (const Label& l : labels) text_shadow(l.text.str(), l.x, l.y, l.colour);
  if (battle.display_mode == kDisplayPeriscope) battle.redraw_setup = 1;
}
ORIGINAL_IN("storm", 0x21808, contact_label_panel);

// $2198c: the contact page's values for the selected ship (a silent or
// gone one: the next contact): its noise as a solution percentage, its
// bearing, speed (identified) and course (on odd frames, loud), a range
// estimate (better the louder it is, off by the sea bed), the depth
// codes (and the engine's sound effect by whether both are on the same
// side of the layer), the signals and sensors each way, its name.
void contact_panel() {
  const int16_t n = battle.selected_ship;
  if (n == 0) return;
  const Ship& s = ships[n];
  if (!(s.flags & kAfloat) || s.noise == 0) {
    next_contact();
    if (battle.selected_ship == -1 || battle.selected_ship != n) {
      battle.redraw_left = 1;
      return;
    }
  }
  text_shadow(common::format_number(int16_t(int16_t(s.noise * 100) >> 8), 2), 40, 147, kPenYellow);
  if (!(s.flags_7 & kPlayerContact)) {
    text_shadow(text_contact_dim.str(), 15, 105, kPenLightGrey);
    text_shadow(common::format_number(0, 3), 15, 140, kPenLightBlue);
    return;
  }
  text_shadow(text_contact_lit.str(), 15, 105, kPenWhite);
  text_shadow(common::format_degrees(int16_t(s.bearing + kHalfTurn)), 15, 126, kPenYellow);
  if ((s.flags_7 & 3) > 1) text_shadow(common::format_number(s.speed, 2), 39, 161, kPenYellow);
  if ((battle.frame_count & 1) && s.noise > 128) text_shadow(common::format_degrees(s.heading), 15, 161, kPenYellow);
  if (s.noise > 64) {
    const int32_t error = int32_t(int16_t(256 - ((s.noise + 31) & ~31)));
    int32_t range = s.range;
    range += int32_t(area_level(s.x, s.y)) * error * range / 256 / 8;
    string_copy(texts().text_buffer, common::number_field(range * 4, 5));
    if (texts().text_buffer[0] == ' ' && texts().text_buffer[1] == ' ') texts().text_buffer[1] = '0';
    texts().text_buffer[2] = 0;
    text_shadow(texts().text_buffer, 15, 175, kPenYellow);
  }
  audio::engine_pitch(s.speed);
  const int16_t own_side = below_limit(ships[0].depth);
  sight.pending_effect = below_limit(s.depth) == own_side ? int16_t(11) : int16_t(12);
  if (s.flags_7 & 3) {
    below_limit(s.depth);
    text_shadow(texts().text_buffer, 51, 168, kPenYellow);
  }
  const int16_t signal = common::clamp(int16_t(s.heard_signal >> 1), 0, 99);
  text_shadow(common::format_number(signal, 3), 15, 140, s.heard_signal < kPenBlack ? kPenLightGrey : kPenLightBlue);
  text_shadow(const_text(sensor_names[s.heard_by]), 33, 140, kPenLightBlue);
  const ShipType& t = ship_types[s.id];
  switch (s.flags_7 & 3) {
    case 1:
    case 2:
      short_name(const_text(class_names[t.kind]));
      text_shadow(texts().text_buffer, 15, 112, kPenYellow);
      break;
    case 3:
      short_name(const_text(type_names[t.name]));
      text_shadow(texts().text_buffer, 15, 112, kPenYellow);
      text_shadow(common::format_number(int16_t(s.sonar_signal >> 1), 3), 39, 140, kPenRed);
      text_shadow(const_text(sensor_names[s.hears_by]), 57, 140, kPenRed);
      break;
    default: break;
  }
}
ORIGINAL_IN("storm", 0x2198c, contact_panel);

// $2d658: one moment of the battle log's replay, from log_view_index to
// the next entry of the player's submarine: the positions marked, the
// ships labelled with their names, their events (launches, damage,
// sinking) as messages. 0 when the submarine's entry is off the map (it
// must be re-centred).
int16_t replay_step() {
  auto screen_of = [](const LogEntry& e, int16_t& x, int16_t& y) {
    x = int16_t((int16_t(e.x - battle.view_x) >> battle.scale) + kMapCentreX);
    y = int16_t((int16_t(e.y - battle.view_y) >> battle.scale) + kMapCentreY);
  };
  constexpr int16_t kWork = 2;
  const int16_t first = battle.log_view_index;
  int16_t x, y;
  screen_of(battle_log[first], x, y);
  if (!in_map_area(x, y)) return 0;
  screen::draw_to(*front_bitmap);
  mark(0, x, y);
  int16_t i = int16_t(first + 1);
  for (int16_t kind = 1; kind <= 3; ++kind) {
    const uint16_t bit = uint16_t(kLogOwn << kind);
    for (; battle_log[i].what & bit; ++i) {
      screen_of(battle_log[i], x, y);
      mark(kind, x, y);
    }
  }
  const int16_t end = i;
  screen::blit_bitmaps(*front_bitmap, kMapLeft, kMapTop, kWork, kMapLeft, kMapTop, 230, 140);
  screen::draw_to(kWork);
  for (i = first; i < end; ++i) {
    const LogEntry& e = battle_log[i];
    if (!(e.what & (kLogOwn | kLogShip))) continue;
    screen_of(e, x, y);
    const int16_t colour = (e.what & kLogOwn) ? kPenWhite : kPenCrimson;
    const int16_t x0 = int16_t(x - 2), y0 = int16_t(y - 2);
    if (!in_map_area(x0, y0) || !in_map_area(int16_t(x0 + 4), int16_t(y0 + 4))) continue;
    screen::plot_colour(x, y, kPenYellow);
    draw_char('[', x0, y0, colour, kWork);
    short_name(const_text(type_names[ship_types[int16_t(e.what & 255)].name]));
    const int16_t half = int16_t(int16_t(int32_t(std::strlen(texts().text_buffer)) * 6) >> 1);
    text_on_bitmap2(texts().text_buffer, int16_t(x0 - half + 3), int16_t(y0 + 6), colour);
  }
  screen::draw_to(*back_bitmap);
  screen::blit_bitmaps(kWork, kMapLeft, kMapTop, *back_bitmap, kMapLeft, kMapTop, 230, 140);
  for (i = first; i < end; ++i) {
    const LogEntry& e = battle_log[i];
    if (!(e.what & (kLogOwn | kLogShip))) continue;
    const int16_t colour = (e.what & kLogOwn) ? kPenYellow : kPenRed;
    const char* name = const_text(type_names[ship_types[int16_t(e.what & 255)].name]);
    const struct {
      uint16_t bit;
      const Const<char>& text;
    } events[] = {{kLogLaunched, text_launches}, {kLogDamaged, text_damaged_by}, {kLogSunk, text_sunk2}};
    for (const auto& ev : events) {
      if (!(e.what & ev.bit)) continue;
      string_copy(texts().text_buffer, name);
      string_append(texts().text_buffer, ev.text.str());
      status_message(texts().text_buffer, colour, 1);
    }
  }
  battle.log_view_index = end;
  return 1;
}
ORIGINAL_IN("storm", 0x2d658, replay_step);

// $2c292: the general quarters report before a battle: the contact's
// bearing, the submarine's heading, speed and depth, the tubes' loadout
// and the reloads, and the local conditions; until a key or fire.
void general_quarters_report() {
  char* buf = texts().text_buffer;
  input::flush_keys();
  screen::draw_to(*back_bitmap);
  screen::fill_rect_colour(0, 0, 319, 199, kPenBlack);
  screen::blit_bitmaps(*back_bitmap, 0, 0, *front_bitmap, 0, 0, 319, 199);
  screen::show_bitmap(*back_bitmap);
  text_style->font = 2;
  const Ship& own = ships[0];
  text(text_sonar_reports.str(), 80, 0, kPenPaleBlue);
  text(text_faint_contact.str(), 80, 8, kPenLightGrey);
  string_copy(buf, text_dots_bearing.str());
  string_append(buf, common::format_degrees(int16_t(contact_bearings[battle_start()->contact_direction] + kHalfTurn)));
  string_append(buf, text_deg3.str());
  text(buf, 80, 16, kPenRed);
  text(text_general_quarters.str(), 80, 32, kPenYellow);
  text(text_xo_reports.str(), 80, 48, kPenPaleBlue);
  string_copy(buf, text_our_heading.str());
  string_append(buf, common::format_degrees(own.heading));
  string_append(buf, text_deg4.str());
  text(buf, 80, 56, kPenLightGrey);
  string_copy(buf, text_our_speed.str());
  string_append(buf, common::format_number(own.speed, 2));
  string_append(buf, text_knots.str());
  text(buf, 80, 64, kPenLightGrey);
  string_copy(buf, text_our_depth.str());
  string_append(buf, common::format_number(own.depth, 3));
  string_append(buf, text_feet2.str());
  text(buf, 80, 72, kPenLightGrey);
  text(text_weapons_officer.str(), 40, 88, kPenPaleBlue);
  text(text_loadout.str(), 40, 96, kPenCyan);
  text(text_loadout_line.str(), 40, 104, kPenCyan);
  for (int16_t t = 0; t < battle.tube_count; ++t) {
    const int16_t x = tube_text_x[t], y = tube_text_y[t];
    string_copy(buf, text_tube2.str());
    string_append(buf, common::format_number(int16_t(t + 1), 1));
    text(buf, x, y, kPenLightGrey);
    text(text_colon.str(), int16_t(x + 49), y, kPenLightGrey);
    string_copy(buf, const_text(weapon_short_names[int16_t(battle.tube_contents[t] & ~kTubeLoaded)]));
    text(buf, int16_t(x + 56), y, kPenPurple);
  }
  if (battle.tube_count != 8) {
    text(text_reloads2.str(), 197, 96, kPenCyan);
    text(text_reloads_line.str(), 197, 104, kPenCyan);
    int16_t line = 4;
    for (int16_t w = 0; w < kWeaponTypes && line < 8; ++w) {
      const int16_t n = battle.weapon_counts[w];
      if (!n) continue;
      text(common::format_number(n, n < 10 ? int16_t(1) : int16_t(2)), 197, tube_text_y[line], kPenPurple);
      text(const_text(weapon_short_names[w]), 218, tube_text_y[line], kPenPurple);
      ++line;
    }
  }
  text(text_local_conditions.str(), 80, 152, kPenPaleBlue);
  text(const_text(sea_mode_names[battle.depth_mode]), 80, 160, kPenLightGrey);
  string_copy(buf, const_text(strength_words[battle.duct_strength]));
  string_append(buf, text_surface_duct.str());
  text(buf, 80, 168, kPenLightGrey);
  string_copy(buf, const_text(strength_words[battle.layer_strength]));
  string_append(buf, text_thermal_at.str());
  string_append(buf, common::format_number(mode_layer_depths[battle.depth_mode], 3));
  string_append(buf, text_feet3.str());
  text(buf, 80, 176, kPenLightGrey);
  text(text_press_continue2.str(), 74, 192, kPenYellow);
  bool done = false;
  while (!done) {
    if (input::getkey() || (input::controls() & input::kFire)) done = true;
  }
  screen::fill_rect_colour(0, 0, 319, 199, kPenBlack);
  text_style->font = 1;
  input::flush_keys();
}
ORIGINAL_IN("storm", 0x2c292, general_quarters_report);

// $2afce: the data base page of a ship type: its picture, maximum speed,
// weapons systems, sonar suite (frequency and kind per sensor) and sound
// level.
void type_data_page(int16_t type) {
  constexpr int16_t kPaper = 16;
  const ShipType& t = ship_types[type];
  screen::fill_rect_colour(74, 15, 302, 25, kPenBlack);
  text_shadow(text_data_base.str(), 118, 18, kPenYellow);
  text_shadow(const_text(type_names[t.name]), 184, 18, kPenWhite);
  screen::fill_rect_colour(kMapLeft, kMapTop, kMapRight, kMapBottom, kPaper);
  screen::draw_masked(type_shapes[type], int16_t(77 + type_shape_dx[type]), int16_t(30 + type_shape_dy[type]), 0, 0);
  text_jam2(text_max_speed.str(), 82, 70, kPenYellow, kPaper);
  text_jam2(common::format_number(t.max_speed, 2), 172, 70, kPenWhite, kPaper);
  text_jam2(text_knots2.str(), 190, 70, kPenWhite, kPaper);
  text_jam2(text_weapons_systems.str(), 82, 86, kPenYellow, kPaper);
  int16_t y = 94;
  auto line = [&](const Const<char>& s) {
    text_jam2(s.str(), 88, y, kPenWhite, kPaper);
    y = int16_t(y + 8);
  };
  if (!t.unknown_30 && !t.weapons) {
    line(text_none4);
  } else {
    if (t.weapons & kHasTorpedoes) line(text_homing_torpedos);
    if (t.weapons & kHasMissiles) line(text_asw_missiles);
    if (t.weapons & kHasRbu) line(text_rbu_rockets);
    if (t.weapons & kHasHelicopter) line(text_asw_helicopter);
  }
  text_jam2(text_sonar_suite.str(), 82, y, kPenYellow, kPaper);
  y = int16_t(y + 8);
  const int16_t first = y;
  const Const<char>* bands[] = {&text_lf, &text_mf, &text_hf};
  const Const<char>* kinds[] = {&text_hull_active, &text_hull_passive, &text_towed_vd};
  for (int16_t k = 0; k < 3; ++k) {
    if (!t.rating[k]) continue;
    const int16_t g = t.guidance[k];
    if (g >= 0 && g <= 2) text_jam2(bands[g]->str(), 88, y, kPenWhite, kPaper);
    text_jam2(kinds[k]->str(), 106, y, kPenWhite, kPaper);
    y = int16_t(y + 8);
  }
  if (y == first) text_jam2(text_none5.str(), 88, y, kPenWhite, kPaper);
  text_jam2(text_sound_level.str(), 82, 158, kPenPaleBlue, kPaper);
  text_jam2(common::format_number(t.noise, 2), 160, 158, kPenPaleBlue, kPaper);
  text_jam2(text_av2.str(), 178, 158, kPenPaleBlue, kPaper);
  const int16_t n = t.noise;
  const Const<char>* level = &text_very_noisy;
  int16_t colour = kPenWhite;
  if (n == 0 || (n >= 27 && n <= 30)) {
    level = &text_very_quiet;
    colour = kPenLightBlue;
  } else if (n >= 31 && n <= 34) {
    level = &text_quiet2;
    colour = kPenPaleBlue;
  } else if (n >= 35 && n <= 37) {
    level = &text_semi_quiet;
    colour = kPenCyan;
  } else if (n >= 38 && n <= 41) {
    level = &text_semi_noisy;
    colour = kPenRed;
  } else if (n >= 42 && n <= 45) {
    level = &text_noisy;
    colour = kPenYellow;
  }
  text_jam2(level->str(), 196, 158, colour, kPaper);
  battle.shown_mode = 9;
}
ORIGINAL_IN("storm", 0x2afce, type_data_page);

namespace {
constexpr int16_t kWork = 2, kFaint = '[', kTrailNoise = 224;
int16_t screen_x(int16_t x) { return int16_t((int16_t(x - battle.view_x) >> battle.scale) + kMapCentreX); }
int16_t screen_y(int16_t y) { return int16_t((int16_t(y - battle.view_y) >> battle.scale) + kMapCentreY); }

// The marks where the ships (the player's, and loud contacts) and the
// running weapons were last drawn, left on the front bitmap as a trail.
void draw_trails() {
  const Ship& own = ships[0];
  for (int16_t k = 0; k < kShips; ++k) {
    const Ship& s = ships[k];
    if (!(s.flags & kAfloat) || (s.events & kEventSunk)) continue;
    if (k != 0 && s.noise < kTrailNoise) continue;
    mark(int16_t(s.side), s.mark_x, s.mark_y);
  }
  for (int16_t i = 0; i < kObjects; ++i) {
    const Object& o = objects[i];
    if (!(o.flags & kRunning)) continue;
    if (o.depth == -1 && o.kind == kEnemyWeapon && own.depth >= kPeriscopeDepth) continue;
    mark(int16_t(o.kind), o.mark_x, o.mark_y);
  }
}

// What the ship pass leaves for the weapon pass (as the original's
// registers did): the last ship's heading and label pen.
struct ShipPass {
  int16_t heading = 0, label_pen = 0;
};

// The player's submarine and the enemy contacts (where the sonar puts
// them, off by the sea bed unless loud): heading arrows or "[" when faint,
// coloured by loudness, the selected one blinking, with their names.
ShipPass draw_ships() {
  Ship& own = ships[0];
  int16_t plot_pen = 0, char_pen = 0;
  ShipPass pass;
  for (int16_t k = 0; k < kShips; ++k) {
    Ship& s = ships[k];
    if (!(s.flags & kAfloat) || (s.events & kEventSunk)) continue;
    if (k != 0 && !(s.flags_7 & kPlayerContact)) continue;
    int16_t mx = s.x, my = s.y;
    if (k != 0) {
      course_to(own.x, own.y, s.x, s.y);
      int32_t range = sight.course_range;
      const int32_t error = int16_t(256 - ((s.noise + 31) & ~31));
      range += int32_t(area_level(s.x, s.y)) * error * range / 256 / 4;
      sight.course_range = int16_t(range);
      mx = int16_t(own.x + common::sin_mul(sight.course_bearing, sight.course_range));
      my = int16_t(own.y - common::cos_mul(sight.course_bearing, sight.course_range));
      if (s.noise > kTrailNoise) {
        mx = s.x;
        my = s.y;
      }
    }
    const int16_t sx = screen_x(mx), sy = screen_y(my);
    pass.heading = s.heading;
    if (s.side == 0) {
      plot_pen = kPenWhite;
      char_pen = pass.label_pen = kPenPaleBlue;
    } else if (s.side == 1) {
      plot_pen = (k == battle.selected_ship && sight.blink) ? kPenYellow : kPenPurple;
      char_pen = pass.label_pen = s.noise >= 192 ? kPenRed : s.noise >= 128 ? kPenCrimson : kPenPurple;
      if ((s.flags & kDetected) && !((k - battle.frame_count) & 7)) char_pen = kPenYellow;
    }
    const int16_t x0 = int16_t(sx - 2), y0 = int16_t(sy - 2);
    if (in_map_area(sx, sy) && !(s.id == 0 && own.depth >= kPeriscopeDepth)) {
      int16_t ch = int16_t(((((pass.heading >> 8) + 7) & 255) >> 4) + 1);
      if (s.noise < 128 && k != 0) ch = kFaint;
      draw_char(ch, x0, y0, char_pen, kWork);
      screen::plot_colour(sx, sy, plot_pen);
      const char* name = nullptr;
      const ShipType& t = ship_types[s.id];
      if (k == 0 || (s.flags_7 & (kIdentified | kPlayerContact)) == (kIdentified | kPlayerContact)) name = const_text(type_names[t.name]);
      else if ((s.flags_7 & (kIdentified | kPlayerContact)) >= 9) name = const_text(class_names[t.kind]);
      if (name) {
        short_name(name);
        const int16_t half = int16_t(int16_t(int32_t(std::strlen(texts().text_buffer)) * 6) >> 1);
        text_on_bitmap2(texts().text_buffer, int16_t(x0 - half + 3), int16_t(y0 + 6), pass.label_pen);
      }
    } else if (k == 0 && (battle.display_mode == 1 || battle.display_mode == 5)) {
      battle.redraw_setup = 1;
    }
    s.mark_x = sx;
    s.mark_y = sy;
    s.mark_heading = pass.heading;
  }
  return pass;
}

// The loud weapons: direction characters (a selected wire-guided torpedo
// blinking). Their stored heading is the last ship's, as in the original.
void draw_weapons(ShipPass pass) {
  const Ship& own = ships[0];
  int16_t label_pen = pass.label_pen;
  for (int16_t i = 0; i < kObjects; ++i) {
    Object& o = objects[i];
    if (!(o.flags & kRunning) || o.noise <= 192) continue;
    if (o.kind == kPlayerWeapon) label_pen = o.seek_time > o.time ? kPenWhite : kPenPaleBlue;
    else if (o.kind == kEnemyWeapon) label_pen = o.seek_time > o.time ? kPenYellow : kPenGreen;
    const int16_t sx = screen_x(o.x), sy = screen_y(o.y);
    if (in_map_area(sx, sy)) {
      text_style->font = 3;
      if (label_pen == kPenWhite && i == battle.selected_object && battle.sub_mode == kPageTorpedo && sight.blink &&
          (o.flags & kWired) && o.weapon == kWireGuided)
        label_pen = kPenDarkGrey + kShadowPens;  // (dark grey's shadow pen)
      const int16_t dir = int16_t(uint16_t(o.heading + kCloseAngle) >> 12);  // 16 directions, rounded
      const int16_t off = dir > 7 ? int16_t(0) : int16_t(4);
      if (!(label_pen == kPenGreen && o.depth == -1 && own.depth >= kPeriscopeDepth))
        draw_char(int16_t('0' + dir), int16_t(sx - off), int16_t(sy - 4), label_pen, kWork);
      text_style->font = 1;
    } else if (i == battle.selected_object && battle.display_mode == 3) {
      battle.redraw_setup = 1;
    }
    o.mark_x = sx;
    o.mark_y = sy;
    o.mark_heading = pass.heading;
  }
}

// The projectiles, on odd frames (their glyph by kind).
void draw_projectiles() {
  int16_t glyph = 0, glyph_pen = 0;
  for (int16_t p = 0; p < kProjectiles; ++p) {
    const Projectile& j = projectiles[p];
    if (!j.time) continue;
    const int16_t sx = screen_x(j.x), sy = screen_y(j.y);
    switch (j.kind) {
      case 1: glyph_pen = kPenPaleBlue; glyph = 17; break;
      case 2: glyph_pen = kPenPaleBlue; glyph = 18; break;
      case 3: glyph_pen = kPenPaleBlue; glyph = 19; break;
      case 4: glyph = glyph_pen = kPenRed; break;
      case 5: glyph_pen = kPenPaleBlue; glyph = 20; break;
      default: break;
    }
    if (!(battle.frame_count & 1)) continue;
    const int16_t x0 = int16_t(sx - 2), y0 = int16_t(sy - 2);
    if (in_map_area(x0, y0) && in_map_area(int16_t(x0 + 4), int16_t(y0 + 4))) draw_char(glyph, x0, y0, glyph_pen, kWork);
  }
}
}  // namespace

// $236b2: the tactical display's frame (1, 3, 5): the trails; then on a
// working copy the ships, the weapons, the projectiles and the aiming
// cursor; then copied to the back bitmap, and a weapon fired when aimed.
void tactical_display() {
  int16_t fire = 0;
  sight.blink ^= 1;
  sight.blink = uint16_t(sight.blink & 1);
  screen::draw_to(*front_bitmap);
  draw_trails();
  screen::blit_bitmaps(*front_bitmap, kMapLeft, kMapTop, kWork, kMapLeft, kMapTop, 231, 141);
  screen::draw_to(kWork);
  draw_weapons(draw_ships());
  draw_projectiles();
  int16_t w = battle.active_weapon;
  if (w != -1 && !battle.aiming && !battle.wire_aimed && objects[w].seek_time < objects[w].time && !(objects[w].flags & kWired)) {
    battle.active_weapon = -1;
    w = -1;
  }
  if (w != -1) {
    fire = aim_cursor();
    screen::draw_to(kWork);
    const Object& o = objects[w];
    if (battle.steer_keys && !ui::state().button_event && ((o.seek_time < o.time && !battle.wire_aimed) || battle.aiming)) {
      move_pointer(sight.aim_x, sight.aim_y, 0);
      tracked_weapon() = w;
    } else {
      steer_torpedo();
    }
  }
  screen::draw_to(*back_bitmap);
  screen::blit_bitmaps(kWork, kMapLeft, kMapTop, *back_bitmap, kMapLeft, kMapTop, 231, 141);
  if (battle.aiming || (w != -1 && objects[w].seek_time < objects[w].time && !battle.wire_aimed)) screen::restore_pointer();
  else screen::hide_pointer();
  if (!battle.aiming || !fire) return;
  fire_weapon();
  if (!battle.aiming) return;
  battle.aiming = 0;
  if (!battle.difficulty) show_event(battle.fired_weapon == kMark48 ? kPictureTorpedoLaunch : kPictureMissileLaunch, 1, battle.own_class, 0);
  string_copy(texts().text_buffer, const_text(weapon_names[battle.fired_weapon]));
  string_append(texts().text_buffer, text_launched_sir.str());
  status_message(texts().text_buffer, kMessageColour, 1);
}
ORIGINAL_IN("storm", 0x236b2, tactical_display);

}  // namespace game::storm
