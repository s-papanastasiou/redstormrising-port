// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Storm (the battle module): the keys during a battle.
#include "game/audio/calls.hpp"
#include "game/input/vars.hpp"
#include "game/mem/register.hpp"
#include "game/screen/calls.hpp"
#include "game/storm/calls.hpp"
#include "game/storm/vars.hpp"
#include "game/ui/state.hpp"
#include "game/storm/texts.hpp"

namespace game::storm {

namespace {
bool tactical() { return battle.display_mode == 1 || battle.display_mode == 3 || battle.display_mode == 5; }

// A message about the selected wire-guided torpedo: when the wire still
// holds, act and say so, else say the wire is broken.
template <typename Act>
void wire_order(int16_t w, const Const<char>& done, const Const<char>& broken, Act act) {
  if (w < 0 || battle.paused) return;
  Object& o = objects[w];
  if (o.flags & kWired) {
    act(o);
    status_message(done.str(), kMessageColour, 1);
  } else {
    status_message(broken.str(), kPenWhite, 1);
  }
}

void on_off(const Const<char>& what, bool on, const Const<char>& yes, const Const<char>& no) {
  string_copy(texts().text_buffer, what.addr());
  string_append(texts().text_buffer, (on ? yes : no).addr());
  status_message(texts().text_buffer, kMessageColour, 1);
}
}  // namespace

// $24684: a key during a battle. A number being typed takes it. On the
// identification list a capital letter opens that type's page; on the
// sonar library it shows that type's shades, and Backspace/Return names
// the selected contact with it (right: identified; wrong: it fades). On
// the periscope Backspace shows the ship in view. Then the keys: displays
// (F1-F8), number entries (F9 depth, F10 course), weapons (1-7: noise
// makers, decoys and the weapons to aim), sensors (8 sonar, 9 radar),
// speed, rudder and course, the torpedo's wire orders (Shift F1-F5,
// Shift F6 cuts it), tubes ($ % ^ &), pages and the game's switches
// (pause, time, sounds, action track, replay, quit). Returns the key (0
// when taken).
int16_t process_key(int16_t key) {
  const int16_t sel = battle.selected_object;
  Ship& own = ships[0];
  if (battle.entry_mode) {
    order_key(int16_t(int8_t(key)));
    return 0;
  }
  if (battle.display_mode == kDisplayIdentify && key >= 'A' && key < 'Z') {
    type_data_page(int16_t(ship_list[key - 'A']));
    key = 0;
  }
  if (battle.display_mode == kDisplayLibrary) {
    if (key >= 'A' && key < 'Z') {
      screen::fill_rect_colour(kShadeX, kShadeY, 300, 138, kPenBlack);
      battle.library_choice = int16_t(key - 'A');
      const int16_t type = int16_t(ship_list[battle.library_choice]);
      shade_column(battle.library_choice, type, kShadeX, kShadeY);
      text_shadow(const_text(type_names[ship_types[type].name]), 212, kShadeY, kPenRed);
      key = 0;
    } else if ((key == 8 || key == 13) && battle.library_choice != -1) {
      Ship& s = ships[battle.selected_ship];
      if (s.id == ship_list[battle.library_choice]) {
        s.flags_7 |= (kIdentified | kPlayerContact);
      } else {
        s.flags_7 &= uint8_t(~3);
        s.noise = 1;
      }
    }
  }
  if (battle.display_mode == kDisplayPeriscope && key == 8 && sight.seen_type != -1) {
    const int16_t t = sight.seen_type;
    screen::fill_rect_colour(75, kMapTop, 302, 76, kPenBlack);
    screen::draw_masked(type_shapes[t], int16_t(77 + type_shape_dx[t]), int16_t(30 + type_shape_dy[t]), 0, 0);
    text_shadow(const_text(type_names[ship_types[t].name]), 90, 70, kPenRed);
    key = 0;
  }
  auto active = [] { return !battle.paused; };
  auto armed = [] { return !battle.paused && !battle.aiming; };
  switch (key) {
    case 'b': enter_mode3(); break;
    case 'c':
      audio::engine_off();
      next_contact();
      break;
    case 'm': enter_mode5(); break;
    case 'n':
      if (battle.aiming) {
        battle.aiming = 0;
        clear_status();
      }
      weapon_page();
      break;
    case 'v': tubes_page(1); break;
    case 'z': change_scale(-1); break;
    case 'x': change_scale(1); break;
    case '1':
      if (active()) launch(kNoisemaker, 0);
      break;
    case '2':
      if (active()) launch(kDecoy, 0);
      break;
    case '3': case '4': case '5': case '6': case '7': {
      if (!active() || !tactical()) break;
      static constexpr int16_t kWeapon[] = {kStinger, kMark48, kSeaLance, kHarpoon};
      start_aiming(key == '7' ? (battle.weapon_counts[kTasm] ? kTasm : kTomahawk) : kWeapon[key - '3']);
      break;
    }
    case '8':
      if (!active()) break;
      own.flags ^= kDetected;
      battle.redraw_helm = 1;
      on_off(text_active_sonar2, own.flags & kDetected, text_on, text_off);
      break;
    case '9':
      if (!active()) break;
      if (own.depth < kPeriscopeDepth && !battle.paused) {
        own.flags ^= kMastRaised;
        battle.redraw_helm = 1;
        on_off(text_active_radar, own.flags & kMastRaised, text_on2, text_off2);
        if (own.flags & kMastRaised) audio::play_effect(kEffectRadar);
      } else {
        status_message(text_too_deep2.str(), kPenWhite, 1);
      }
      break;
    case '0':
      if (!active()) break;
      own.speed_set = int16_t(ship_types[own.id].max_speed / 6);
      own.flags &= uint8_t(~(kDetected | kMastRaised));
      battle.redraw_own_ship = 1;
      battle.redraw_helm = 1;
      status_message(text_silent_running.str(), kMessageColour, 1);
      break;
    case '+': case '=':
      if (active()) change_speed(1);
      break;
    case '-': case '_':
      if (active()) change_speed(-1);
      break;
    case ',': case '<':
      if (active()) change_rudder(-1);
      break;
    case '.': case '>':
      if (active()) change_rudder(1);
      break;
    case '/': case '?':
      if (!active()) break;
      own.course = 0;
      own.depth_set = 0;
      battle.rudder = 0;
      battle.redraw_helm = 1;
      status_message(text_straight_level.str(), kMessageColour, 1);
      break;
    case '$':
      if (armed()) load_tube(1);
      break;
    case '%':
      if (armed()) load_tube(2);
      break;
    case '^':
      if (armed()) load_tube(3);
      break;
    case '&':
      if (armed()) load_tube(4);
      break;
    case 27:
      if (battle.aiming) {
        battle.aiming = 0;
        clear_status();
      } else if (battle.sub_mode == kPageTorpedo && tactical() && battle.active_weapon != -1) {
        battle.wire_aimed = 1;
      }
      break;
    case input::kKeyShiftF1:
      wire_order(sel, text_run_shallow, text_wire_broken1, [](Object& o) { o.depth = kRunShallow; });
      break;
    case input::kKeyShiftF2:
      wire_order(sel, text_run_deep, text_wire_broken2,
                 [](Object& o) { o.depth = int16_t(battle.layer_depth + kRunDeeper); });
      break;
    case input::kKeyShiftF3:
      wire_order(sel, text_left_search, text_wire_broken3, [](Object& o) {
        o.flags &= uint8_t(~kTurnsRight);
        if (o.seek_time > o.time) o.steered = 3;
        battle.redraw_left = 1;
      });
      break;
    case input::kKeyShiftF4:
      wire_order(sel, text_right_search, text_wire_broken4, [](Object& o) {
        o.flags |= kTurnsRight;
        if (o.seek_time > o.time) o.steered = 3;
        battle.redraw_left = 1;
      });
      break;
    case input::kKeyShiftF5:
      wire_order(sel, text_torpedo_activated, text_wire_broken5, [](Object& o) {
        o.seek_time = kNeverSeek;
        o.steered = 1;
      });
      break;
    case input::kKeyShiftF8:
      if (!tactical()) break;
      battle.depth_chart_on = uint16_t(battle.depth_chart_on ^ 1);
      battle.redraw_setup = 1;
      break;
    case input::kKeyShiftF6:
      if (sel < 0 || battle.paused) break;
      remove_object(sel);
      status_message(text_wire_cut.str(), kMessageColour, 1);
      break;
    case input::kKeyF1: enter_display1(); break;
    case input::kKeyF2: enter_mode3b(); break;
    case input::kKeyF3: enter_display5(); break;
    case input::kKeyF4:
      refresh_status();
      periscope_display();
      break;
    case input::kKeyF5:
      refresh_status();
      sonar_library_display();
      break;
    case input::kKeyF6:
      refresh_status();
      sonar_analysis_display();
      break;
    case input::kKeyF7:
      refresh_status();
      letters_display();
      break;
    case input::kKeyF8:
      refresh_status();
      sonar_display();
      break;
    case input::kKeyF9:
      if (!active()) break;
      refresh_status2();
      start_message(kEntryDepth);
      break;
    case input::kKeyF10:
      if (!active()) break;
      refresh_status2();
      start_message(kEntryCourse);
      break;
    case input::kKeyAltA:
      battle.action_track ^= 1;
      status_message((battle.action_track ? text_track_on : text_track_off).str(), kPenWhite, 1);
      status_if_flag();
      break;
    case input::kKeyHelp: tactical_analysis(); break;
    case input::kKeyAltP:
      battle.paused = battle.paused ? uint16_t(0) : uint16_t(1);
      if (battle.paused) {
        battle.entry_mode = 0;
        battle.aiming = 0;
        audio::play_effect(kEffectSilence);
        audio::engine_off();
        flash_red();
      } else {
        audio::play_effect(kEffectSoundOn);
        flash_black();
      }
      status_message((battle.paused ? text_paused : text_resume).str(), kPenWhite, 1);
      break;
    case input::kKeyAltQ: ui::state().quit_key = 1; break;
    case input::kKeyAltR: replay_battle(); break;
    case input::kKeyAltT:
      battle.time_rate = battle.time_rate == kAcceleratedTime ? kRealTime : kAcceleratedTime;
      status_message((battle.time_rate == kAcceleratedTime ? text_accelerated : text_real_time).str(), kPenWhite, 1);
      status_if_flag();
      break;
    case input::kKeyAltV:
      battle.sounds_on = battle.sounds_on ? uint16_t(0) : uint16_t(1);
      status_message((battle.sounds_on ? text_sounds_on : text_sounds_off).str(), kPenWhite, 1);
      status_if_flag();
      if (battle.sounds_on) {
        audio::play_effect(kEffectSoundOn);
      } else {
        audio::play_effect(kEffectSilence);
        audio::engine_off();
        audio::play_effect(kEffectSilenceAll);
      }
      break;
    case input::kKeyAltZ: set_display(battle.display_mode, battle.sub_mode); break;
    default:
      if (!battle.aiming && !battle.entry_mode && !battle.paused) clear_status();
      break;
  }
  return key;
}
ORIGINAL_IN("storm", 0x24684, process_key);

}  // namespace game::storm
