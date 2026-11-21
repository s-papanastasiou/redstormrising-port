// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Storm (the battle module): the per-step changes of each ship (heading,
// depth, noise) and the reloading of the tubes.
#include "game/audio/calls.hpp"
#include "game/common/math.hpp"
#include "game/common/strings.hpp"
#include "game/input/calls.hpp"
#include "game/input/vars.hpp"
#include "game/mem/register.hpp"
#include "game/screen/calls.hpp"
#include "game/storm/calls.hpp"
#include "game/storm/vars.hpp"
#include "game/ui/state.hpp"
#include "game/storm/texts.hpp"

namespace game::storm {

// $2cdf2: two records each keep the highest level asked for (ties: the
// latest) with its value.
void keep_highest(int16_t level, int16_t value) {
  if (level >= battle.highest_a) {
    battle.highest_a = level;
    battle.highest_a_value = value;
  }
  if (level >= battle.highest_b) {
    battle.highest_b = level;
    battle.highest_b_value = value;
  }
}
ORIGINAL_IN("storm", 0x2cdf2, keep_highest);

// $2cfac: record a moves into record b and is cleared.
void roll_highest() {
  battle.highest_b = battle.highest_a;
  battle.highest_b_value = battle.highest_a_value;
  battle.highest_a = 0;
  battle.highest_a_value = 0;
}
ORIGINAL_IN("storm", 0x2cfac, roll_highest);

// $297be: object i is gone.
void remove_object(int16_t i) {
  Object& o = objects[i];
  o.state = 0;
  o.flags = 0;
}
ORIGINAL_IN("storm", 0x297be, remove_object);

// $25ed8: ship n's turn this step (twice its speed, towards its course
// with a slow weave; the result) and, below the surface, its depth moving
// towards the depth its speed holds it at.
int32_t trim_step(int16_t n) {
  Ship& s = ships[n];
  const int16_t weave = int16_t((((battle.frame_count >> 3) & 32) - 16) << 8);
  const int16_t towards = int16_t(s.course + weave);
  const int32_t turn = int32_t(int16_t(s.speed * 2)) * common::sign(int16_t(towards - s.heading));
  if (s.depth > 0) {
    const int16_t target =
        common::clamp(int16_t(45 * (s.speed - 13)), int16_t(battle.layer_depth - 15), kMaxTrimDepth);
    s.depth = int16_t(s.depth + int16_t(common::sign(int16_t(target - s.depth)) << (3 - battle.time_shift)));
  }
  return turn;
}
ORIGINAL_IN("storm", 0x25ed8, trim_step);

// $25dca: ship n's sounds and noise this step: a detected ship pings; a
// ship flagged once (bit 4 of flags_7) is not quieter this step (for the
// player near the surface: keep_highest(5, 0)), else its noise falls. A
// hard turn at speed makes every contact undetected again.
void ship_step(int16_t n) {
  Ship& s = ships[n];
  if (s.flags & kDetected) audio::play_effect(kEffectOwnPing);
  if (s.flags_7 & kTracksPlayer) {
    s.flags_7 &= uint8_t(~kTracksPlayer);
    if (n == 0 && s.depth < battle.layer_depth) keep_highest(5, 0);
  } else {
    s.noise = common::clamp(int16_t(s.noise - (4 >> battle.time_shift)), 0, 255);
  }
  if (int16_t(s.speed + 5 * common::abs_saturated(battle.rudder) / 2) <= 20) return;
  for (int16_t i = 0; i < kObjects; ++i) {
    Object& o = objects[i];
    if (o.kind == kPlayerWeapon) o.flags &= uint8_t(~kWired);
  }
}
ORIGINAL_IN("storm", 0x25dca, ship_step);

// $29414: the tubes being loaded count down; a loaded tube is marked
// (bit 7) with a message. With an empty tube and weapons left,
// keep_highest(13, the tube's number).
void reload_tick() {
  for (int16_t i = 0; i < battle.tube_count; ++i) {
    int16_t& tube = battle.tube_contents[i];
    if (tube == 0 || (tube & kTubeLoaded)) continue;
    battle.load_times[i] -= 1;
    if (battle.load_times[i] != 0) continue;
    string_copy(texts().text_buffer, const_text(weapon_names[int16_t(tube & ~kTubeLoaded)]));
    string_append(texts().text_buffer, text_loaded_in_tube.str());
    string_append(texts().text_buffer, common::format_number(int16_t(i + 1), 1));
    string_append(texts().text_buffer, text_comma_sir.str());
    status_message(texts().text_buffer, kMessageColour, 1);
    tube = int16_t(tube | kTubeLoaded);
  }
  int16_t empty = 0;
  while (battle.tube_contents[empty] != 0 && empty < battle.tube_count) ++empty;
  int16_t left = 0;
  for (int16_t w = 1; w < 8; ++w) left = int16_t(left + battle.weapon_counts[w]);
  if (battle.tube_count != empty && left != 0) keep_highest(13, int16_t(empty + 1));
  if (battle.sub_mode == kPageWeapons) battle.redraw_right = 1;
}
ORIGINAL_IN("storm", 0x29414, reload_tick);

// $2683e: object i turns towards its course, 1/32 turn per full step;
// within that it takes the course, which is then done.
void approach(int16_t i) {
  Object& o = objects[i];
  const int16_t d = int16_t(o.course - o.heading);
  if (common::abs_saturated(d) < kCloseAngle) {
    o.heading = o.course;
    o.course = 0;
    return;
  }
  o.heading = int16_t(o.heading + (common::sign(d) << (11 - battle.time_shift)));
}
ORIGINAL_IN("storm", 0x2683e, approach);

// $2664e: object i moves: its speed towards the set speed, bit 0 of its
// flags while it is shallower than its speed holds it, its position along
// its heading; a torpedo that hits the sea bed or the ice is gone (with a
// message).
void object_step(int16_t i) {
  Object& o = objects[i];
  o.speed = int16_t(o.speed + common::sign(int16_t(o.speed_set - o.speed)));
  if (o.depth < int16_t(45 * (o.speed - 13))) o.flags |= kCavitating;
  else o.flags &= uint8_t(~kCavitating);
  const int16_t distance = int16_t(o.speed >> battle.time_shift);
  const int16_t odd = int16_t(battle.frame_count & 1);
  o.x = int16_t(o.x + (int16_t(common::sin_mul(o.heading, distance) + odd) >> 1));
  o.y = int16_t(o.y - (int16_t(common::cos_mul(o.heading, distance) + odd) >> 1));
  if (o.depth <= 0) return;
  auto hit = [&](const Const<char>& text) {
    remove_object(i);
    if (o.kind != kPlayerWeapon) return;
    status_message(text.str(), kPenCyan, 1);
    audio::play_effect(kEffectExplosionDeep);
  };
  if (battle.depth_mode == 0 && o.depth > depth_at(o.x, o.y)) hit(text_torpedo_bottom);
  if (battle.depth_mode == 1 && o.depth < depth_at(o.x, o.y)) hit(text_torpedo_iceberg);
}
ORIGINAL_IN("storm", 0x2664e, object_step);

// $29df2: in sub-mode 4, the selected wire-guided torpedo is steered
// while its wire reaches: by the mouse buttons (select: left, menu:
// right) or else the keys; twice as fast in accelerated time.
void steer_torpedo() {
  int16_t dirs;
  if (ui::state().select_button != 0) {
    input::read_stick();
    dirs = ui::state().select_button == 1 ? input::kLeft : ui::state().select_button == 2 ? input::kRight : 0;
  } else {
    dirs = battle.steer_keys;
  }
  const int16_t sel = battle.selected_object;
  if (battle.sub_mode != kPageTorpedo || sel < 0) return;
  Object& o = objects[sel];
  if (!(o.flags & kWired) || o.weapon != kMark48 || o.time >= o.seek_time) return;
  const int16_t step = int16_t((battle.time_rate == kAcceleratedTime ? 2 : 1) << (10 - battle.time_shift));
  if (dirs & input::kRight) {
    o.steered = 1;
    o.heading = int16_t(o.heading + step);
    o.course = o.heading == 0 ? int16_t(1) : o.heading;
  }
  if (dirs & input::kLeft) {
    o.steered = 1;
    o.heading = int16_t(o.heading - step);
    o.course = o.heading == 0 ? int16_t(1) : o.heading;
  }
}
ORIGINAL_IN("storm", 0x29df2, steer_torpedo);

// $25ba6: the projectiles' time runs down; those of kind 2 move on odd
// frames along their heading.
void move_projectiles() {
  for (int16_t i = 0; i < kProjectiles; ++i) {
    Projectile& p = projectiles[i];
    if (p.time == 0) continue;
    p.time = int16_t(p.time - (4 >> battle.time_shift));
    if (p.kind != 2 || !(battle.frame_count & 1)) continue;
    const int16_t distance = int16_t(20 >> battle.time_shift);
    p.x = int16_t(p.x + common::sin_mul(p.heading, distance));
    p.y = int16_t(p.y - common::cos_mul(p.heading, distance));
  }
}
ORIGINAL_IN("storm", 0x25ba6, move_projectiles);

// $23094: a projectile of this kind from the ship: a free one (for a
// knuckle any knuckle); none: a message (noisemakers and decoys).
// Returns its number (kProjectiles: none).
int16_t launch(int16_t kind, int16_t ship) {
  int16_t i = 0;
  for (; i < kProjectiles; ++i) {
    const Projectile& p = projectiles[i];
    if (p.time == 0 && p.kind == kind) break;
    if (kind == kKnuckle && p.kind == kKnuckle) break;
  }
  if (i >= kProjectiles) {
    if (kind == kDecoy || kind == kNoisemaker) {
      string_copy(texts().text_buffer, text_no.str());
      string_append(texts().text_buffer, const_text(projectile_names[kind]));
      string_append(texts().text_buffer, text_available.str());
      status_message(texts().text_buffer, kPenWhite, 1);
    }
    return i;
  }
  Projectile& p = projectiles[i];
  Ship& s = ships[ship];
  p.x = s.x;
  p.y = s.y;
  p.depth = s.depth;
  p.heading = s.heading;
  p.time = 128;
  if (kind == kNoisemaker) {
    s.noise = common::clamp(int16_t(s.noise + 32), 0, 255);
    p.time = 200;
    status_message(text_noisemaker.str(), kMessageColour, 1);
    audio::play_effect(kEffectCountermeasure);
  } else if (kind == kDecoy) {
    p.time = 400;
    status_message(text_decoy.str(), kMessageColour, 1);
    audio::play_effect(kEffectCountermeasure);
  } else if (kind == kKnuckle) {
    p.time = 128;
    status_message(text_knuckle.str(), kMessageColour, 1);
  }
  return i;
}
ORIGINAL_IN("storm", 0x23094, launch);

// $28d62: ship n is hit by a weapon of this kind: a sound, then a roll
// against its toughness (doubled for kinds 2 and 3, halved for type kind
// 1, times the setting + 3): low enough it sinks, else it is damaged (and
// sinks three times as easily from then on).
void hit_ship(int16_t n, int16_t kind) {
  Ship& s = ships[n];
  if (!battle.action_track) audio::play_effect(ship_types[s.id].kind == kSubmarineKind ? kEffectExplosionDeep : kEffectExplosion);
  const int16_t odds = (s.flags & kDamaged) ? int16_t(96) : int16_t(32);
  int16_t strength = int16_t(30 + ship_types[s.id].toughness);
  if (kind == 2 || kind == 3) strength = int16_t(strength * 2);
  if (ship_types[s.id].kind == kSubmarineKind) strength = int16_t(strength >> 1);
  if (common::rnd(int16_t(strength * (battle.difficulty + 3))) < odds * 6) {
    s.events |= kEventSunk;
  } else {
    s.flags |= kDamaged;
    s.events |= kEventDamaged;
  }
}
ORIGINAL_IN("storm", 0x28d62, hit_ship);

// $2422e: every frame: the steering keys are read; with a weapon in the
// water on a tactical display, it is aimed (the pointer follows the aim
// while the keys steer it) or steered, and fired when aim_cursor says so,
// with its sound and message.
void weapon_step() {
  battle.steer_keys = int16_t(input::controls());
  if (battle.active_weapon == -1) return;
  const int16_t m = battle.display_mode;
  if (m != 1 && m != 3 && m != 5) return;
  const int16_t x = sight.aim_x, y = sight.aim_y;
  const int16_t fire = aim_cursor();
  const Object& o = objects[battle.active_weapon];
  if ((o.seek_time < o.time && !battle.wire_aimed) || battle.aiming) {
    if (battle.steer_keys && !ui::state().button_event && battle.active_weapon == tracked_weapon()) move_pointer(x, y, 0);
  } else {
    steer_torpedo();
  }
  screen::draw_to(*back_bitmap);
  if (!battle.aiming || !fire) return;
  fire_weapon();
  if (!battle.aiming) return;
  battle.aiming = 0;
  if (!battle.difficulty) show_event(battle.fired_weapon == kMark48 ? kPictureTorpedoLaunch : kPictureMissileLaunch, 1, battle.own_class, 0);
  string_copy(texts().text_buffer, const_text(weapon_names[battle.fired_weapon]));
  string_append(texts().text_buffer, text_launched.str());
  status_message(texts().text_buffer, kMessageColour, 1);
}
ORIGINAL_IN("storm", 0x2422e, weapon_step);

// $29f56: the aimed weapon is fired: from its tube (or the store), into
// its object slot, starting where the submarine is. Missiles (types 2-5)
// need some range; under the ice a weapon other than a torpedo may be
// destroyed. A torpedo is slow and runs long; the others leave at once on
// the course given, and the first enemy at the surface hears the launch.
void fire_weapon() {
  input::flush_keys();
  const int16_t slot = battle.fire_slot;
  const int16_t type = battle.fired_weapon;
  if (type >= 2 && type <= 5 && battle.fire_range < kMinMissileRange) {
    battle.aiming = 0;
    status_message(text_min_range.str(), kPenWhite, 1);
    return;
  }
  if (battle.fire_tube >= 0) battle.tube_contents[battle.fire_tube] = 0;
  else battle.weapon_counts[type] -= 1;
  Ship& own = ships[0];
  if (battle.depth_mode == 1 && type != kWireGuided) {
    const int16_t roll = common::rnd(8);
    if (roll > area_level(own.x, own.y) + 3) {
      battle.aiming = 0;
      string_copy(texts().text_buffer, const_text(weapon_names[type]));
      string_append(texts().text_buffer, text_destroyed_by_ice.str());
      status_message(texts().text_buffer, kPenWhite, 1);
      audio::play_effect(kEffectExplosion);
      return;
    }
  }
  Object& o = objects[slot];
  o.tube = battle.fire_tube;
  o.id = 2;
  o.x = own.x;
  o.y = own.y;
  o.depth = own.depth;
  o.speed = own.speed;
  o.heading = own.heading;
  o.unknown_6 = 0;
  o.course = battle.fire_course ? battle.fire_course : int16_t(1);
  o.mark_x = 0;
  o.steered = 1;
  o.homing = 0;
  o.weapon = uint16_t(type);
  o.state = 0;
  if (type == kWireGuided) {
    o.flags = kRunning | kWired | kTurnsRight;
    o.speed_set = 40;
    o.time = kTorpedoTime;
  } else {
    o.flags = kRunning | kTurnsRight;
    o.speed_set = 300;
    o.time = type == kSeaLance ? int16_t(900) : int16_t(450);
    if (type == kStinger) {
      o.time = 80;
      o.speed_set = 150;
    }
    o.depth = -1;
    o.heading = o.course;
    for (int16_t i = 0; i < kShips; ++i) {
      Ship& s = ships[i];
      if ((!s.events && !s.flags) || s.side != 1 || s.depth >= 1) continue;
      own.noise = 255;
      sight.launch_x = own.x;
      sight.launch_y = own.y;
      s.flags_7 |= kTracksPlayer;
      break;
    }
  }
  if (slot & 1) o.flags &= uint8_t(~kTurnsRight);
  o.speed = o.speed_set;
  o.noise = 255;
  o.seek_time = int16_t(o.time - int16_t(int32_t(battle.fire_range) * 8 / o.speed_set));
  if (o.weapon == kStinger) o.seek_time = 999;
  battle.active_weapon = -1;
  if (o.weapon == kMark48 || o.weapon == kSeaLance) {
    battle.selected_object = slot;
    if (battle.sub_mode == kPageTorpedo) battle.redraw_left = 1;
  }
  own.events |= kEventLaunched;
  if (own.id != kOwnClassQuiet) own.flags |= kNoisy;
  battle.quiet_frames = 0;
  if (!battle.action_track || battle.difficulty) audio::play_effect(kEffectLaunch);
}
ORIGINAL_IN("storm", 0x29f56, fire_weapon);

// $2a2e0: enemy ship n fires at `heading` (a * b / 256 is the range),
// unless an enemy or this ship fired too recently (less so at higher
// settings): a torpedo within its type's reach, else a missile, if the
// type has them, in a free enemy weapon slot.
void enemy_launch(int16_t n, int16_t heading, int16_t a, int16_t b) {
  if (int16_t(battle.frame_count - battle.last_enemy_launch) < int16_t((5 - battle.difficulty) << 5)) return;
  Ship& s = ships[n];
  if (int16_t(battle.frame_count - s.last_launch) < int16_t((6 - battle.difficulty) << 5)) return;
  int16_t slot = 0;
  while (slot < kObjects && !(!objects[slot].state && !objects[slot].flags && objects[slot].kind == kEnemyWeapon)) ++slot;
  if (slot >= kObjects) return;
  Object& o = objects[slot];
  if (o.state || o.flags) return;
  const int16_t range = int16_t((int32_t(a) * b) >> 8);
  o.depth = 50;
  const ShipType& t = ship_types[s.id];
  const int16_t reach =
      int16_t(((battle.contact_flags & kUnderIce) << 7) - ((t.weapons & kHasMissiles) ? kMissileShorter : 0) + kTorpedoReach);
  int16_t kind = 0;
  if (range < reach) {
    if (t.weapons & 1) kind = kEnemyTorpedo;
  } else if (t.weapons & kHasMissiles) {
    kind = kEnemyMissile;
  }
  if (!kind) return;
  o.weapon = uint16_t(kind);
  o.state = 0;
  o.flags = kRunning | kWired | kTurnsRight;
  o.x = s.x;
  o.y = s.y;
  o.heading = s.heading;
  o.noise = s.noise;
  o.unknown_6 = 2;
  o.course = heading;
  o.mark_x = 0;
  o.homing = 0;
  if (kind == kEnemyTorpedo) {
    o.id = 3;
    o.speed_set = 35;
    o.time = 800;
    o.steered = 2;
    const int16_t run = int16_t(o.time - int16_t(int16_t(range / o.speed_set) << 2));
    const int16_t turn = int16_t(common::abs_saturated(int16_t(heading - s.heading)) >> 10);
    o.seek_time = int16_t(((run - turn) & ~63) | 32);
    if (range < 128 && b > 192) {
      o.steered = 3;
      o.seek_time = kNeverSeek;
    }
    s.flags |= kNoisy;
  } else {
    o.id = 4;
    o.speed_set = 400;
    o.seek_time = 500;
    o.time = int16_t(o.seek_time + range / (o.speed_set >> 3) - 16);
    o.steered = 1;
    o.depth = -1;
    o.heading = o.course;
    o.unknown_6 = 0;
    o.noise = 0;
  }
  o.speed = o.speed_set;
  s.last_launch = battle.frame_count;
  battle.last_enemy_launch = battle.frame_count;
  s.events |= kEventLaunched;
}
ORIGINAL_IN("storm", 0x2a2e0, enemy_launch);

// $28198: the observer's sonar on the target (the player's of a ship, or
// a ship's of the player). Heard: the target gets louder; a new contact
// of the player is announced, a known one is identified further as its
// signal allows (its class, then its type, announced); an enemy remembers
// where a loud player was. Not heard: the player's contact fades (and is
// announced lost); an enemy loses the player unless it is detected.
void detect(int16_t observer, int16_t target) {
  Ship& so = ships[observer];
  Ship& st = ships[target];
  const bool player = observer == 0;
  const int16_t signal = player ? st.heard_signal : so.sonar_signal;
  const int16_t sensor = player ? st.heard_by : so.hears_by;
  auto announce = [&](const char* name) { status_bearing(name, int16_t(st.bearing + kHalfTurn), kPenYellow); };
  if (signal > 0) {
    if (player) battle.quiet_frames = 0;
    if (sensor != 0) {
      const int16_t louder = int16_t((signal >> 3) + (st.flags_7 & 3) + 1);
      st.noise = int16_t(st.noise + (int16_t(louder << (2 - battle.time_shift)) >> 1));
      const bool fresh = player ? !(st.flags_7 & kPlayerContact) : !(so.flags_7 & kTracksPlayer);
      if (fresh) {
        if (player) {
          if (ship_types[st.id].kind == kAircraftKind && ships[0].depth < kPeriscopeDepth) st.flags_7 |= 3;
          announce(text_contact.str());
          battle.contact_flags |= kNewContact;
          if (battle.sub_mode == kPageContacts && battle.selected_ship <= 0) {
            next_contact();
            battle.redraw_left = 1;
          }
        }
      } else if ((battle.difficulty != 3 || so.side == 1) && (st.flags_7 & 3) != 3) {
        const int16_t level = common::clamp(int16_t(int16_t((st.noise >> 4) + signal - 16) >> 3), 0, 3);
        if (level > (st.flags_7 & 3)) {
          st.heard = 0;
          st.flags_7 = uint8_t((st.flags_7 & uint8_t(~kIdentified)) + level);
          const ShipType& t = ship_types[st.id];
          if (player && level == 1) announce(const_text(class_names[t.kind]));
          if (player && level == 3) announce(const_text(type_names[t.name]));
        }
      }
      st.noise = common::clamp(st.noise, 0, 255);
    }
    if (so.side == 1) {
      so.flags_7 |= kTracksPlayer;
      if (st.noise > 240) {
        sight.launch_x = st.x;
        sight.launch_y = st.y;
      }
    } else {
      st.flags_7 |= kPlayerContact;
    }
    return;
  }
  if (player) {
    st.noise = common::clamp(int16_t(st.noise - (4 >> battle.time_shift)), 0, 255);
    if ((st.flags_7 & kPlayerContact) && st.side == 1 && st.noise > 64) announce(text_contact_lost.str());
    st.flags_7 &= uint8_t(~kPlayerContact);
    return;
  }
  if (!(st.flags & kDetected)) so.flags_7 &= uint8_t(~kTracksPlayer);
}
ORIGINAL_IN("storm", 0x28198, detect);

// $235d8: whether a depth is below the shallow depth; its code (24 or
// 25) is left in text_buffer as a one-character string.
int16_t below_limit(int16_t depth) {
  texts().text_buffer[1] = 0;
  const bool below = depth < battle.layer_depth;
  texts().text_buffer[0] = below ? kBelowCode : kAboveCode;
  return below ? 1 : 0;
}
ORIGINAL_IN("storm", 0x235d8, below_limit);

// $2326c: the score (-99..99) of an attack by `attacker` on `target`
// (ships) with a weapon kind, fired on `heading` at `range`: how well
// the target can be heard (kinds 1 and 2) or the weapon's rating against
// its defence, the sea bed at both, the aspect, against a distance term
// (logarithmic, by the weapon's guidance). Out of the weapon's arc it
// cannot fire (-99); from the surface on a submerged target it always
// can (99).
int16_t attack_score(const Body& a, const Body& t, int16_t kind, int16_t heading, int16_t range, int16_t bonus) {
  if (a.depth <= 0 && t.depth < 0) return kSureScore;
  const ShipType& ta = ship_types[a.id];
  const ShipType& tt = ship_types[t.id];
  int16_t score;
  if (kind == 1 || kind == 2) {
    score = tt.noise;
    if (t.speed > 5) score = int16_t(score + ((t.speed - 6) >> 1));
    if (t.flags & kCavitating) score = int16_t(score + 3);
    if (t.flags & kNoisy) score = int16_t(score + 3);
    int16_t sensor;
    if (kind == 1) {
      sensor = int16_t(int16_t(ta.noise * 5) >> 3);
      if (a.speed > 5) sensor = int16_t(sensor + ((a.speed - 6) >> 1));
      if (a.flags & kCavitating) sensor = int16_t(sensor + 4);
      if (battle.ambient_noise > sensor) sensor = battle.ambient_noise;
    } else {
      sensor = battle.ambient_noise;
      if (a.speed > 10) sensor = int16_t(sensor + (a.speed >> 1) - 5);
    }
    score = int16_t(score - sensor);
    if (battle.difficulty == 0 && a.side == 0) score = int16_t(score + 2);
    score = int16_t((score + ta.rating[kind]) * 2);
  } else {
    score = int16_t(ta.rating[kind] + bonus - battle.ambient_noise * 2);
    if ((ta.kind == kSubmarineKind || ta.kind == kSurfaceShipKind) && a.speed > 16) score = int16_t(score - (a.speed - 16));
    score = int16_t(score + tt.defence + 6);
    if (battle.depth_mode == 1 && ta.guidance[kind] != 2) score = int16_t(score - 16);
  }
  score = int16_t(score + area_level(a.x, a.y));
  score = int16_t(score + area_level(t.x, t.y));
  const int16_t off_bow = int16_t(common::abs_saturated(int16_t(heading - t.heading)) >> 8);
  int16_t aspect = int16_t(2 - (common::abs_saturated(int16_t(64 - off_bow)) >> 4));
  if (kind == 0) aspect = int16_t(aspect * 2);
  score = int16_t((score + aspect) * 2);
  if ((common::abs_saturated(int16_t(heading - a.heading)) >> 8) > ta.arc[kind]) score = kNoShot;

  int16_t distance = range;
  const int16_t a_below = kind == 2 ? int16_t(0) : below_limit(a.depth);
  const int16_t t_below = below_limit(t.depth);
  if (a_below && t_below) distance = int16_t(int16_t(distance / battle.duct_strength) * 2);
  if (a_below != t_below) distance = int16_t((distance >> 1) * battle.layer_strength);
  distance = common::log_scale(distance);
  distance = int16_t(distance - (distance >> 2));
  const int16_t guidance = ta.guidance[kind];
  if (guidance == 0) distance = int16_t(distance + (range >> 9));
  if (guidance == 1) distance = int16_t(distance + (range >> 8));
  if (guidance == 2) distance = int16_t(distance + (range >> 5));
  if (kind == 0) distance = int16_t(distance + (distance >> 4));
  return common::clamp(int16_t(score - distance + 50), -kSureScore, kSureScore);
}
ORIGINAL_INSIDE(attack_score, 0x2326c, 0, 0, 0, 0);

// $25ada: each sonobuoy (projectile kind 5) in the water listens for the
// player; when it hears (attack_score above noise_floor) the player is
// pinged: loud, its position known, a message and a sound.
void attack_check() {
  Ship& own = ships[0];
  for (int16_t i = 0; i < kProjectiles; ++i) {
    Projectile& p = projectiles[i];
    if (p.kind != 5 || p.time == 0) continue;
    course_to(p.x, p.y, own.x, own.y);
    if (attack_score(body(p), body(own), 0, sight.course_bearing, sight.course_range, 0) <= battle.noise_floor)
      continue;
    own.noise = 255;
    sight.launch_x = own.x;
    sight.launch_y = own.y;
    status_bearing(text_sonobuoy.str(), int16_t(sight.course_bearing + kHalfTurn), kPenYellow);
    audio::play_effect(kEffectSonobuoy);
  }
}
ORIGINAL_IN("storm", 0x25ada, attack_check);

// $2a668: weapon w is aimed at the cursor's point.
void save_aim(int16_t w) {
  AimPoint& p = battle.aim_points[w];
  p.map_x = sight.aim_map_x;
  p.map_y = sight.aim_map_y;
  p.range = sight.aim_range;
  p.bearing = sight.aim_bearing;
}
ORIGINAL_IN("storm", 0x2a668, save_aim);

// $2a6ea: the cursor put on the aim point's place on the map (kept within
// (74..295, 28..159); the point is rounded to the scale's pixels then).
// The pointer follows it while a weapon is aimed or still steerable.
void aim_to_screen() {
  constexpr int16_t kLeft = 74, kRight = 295, kTop = 28, kBottom = 159;
  const int16_t sx = int16_t((int16_t(sight.aim_map_x - battle.view_x) >> battle.scale) + kMapCentreX);
  const int16_t sy = int16_t((int16_t(sight.aim_map_y - battle.view_y) >> battle.scale) + kMapCentreY);
  int16_t x = sx, y = sy;
  if (sx > kRight || sx < kLeft) {
    x = sx > kRight ? kRight : kLeft;
    sight.aim_map_x = int16_t(int16_t(int16_t(sx - kMapCentreX) << battle.scale) + battle.view_x);
  }
  if (sy > kBottom || sy < kTop) {
    y = sy > kBottom ? kBottom : kTop;
    sight.aim_map_y = int16_t(int16_t(int16_t(sy - kMapCentreY) << battle.scale) + battle.view_y);
  }
  sight.aim_x = x;
  sight.aim_y = y;
  const int16_t w = battle.active_weapon;
  if (w == -1) return;
  const Object& o = objects[w];
  if ((o.seek_time < o.time && !battle.wire_aimed) || battle.aiming) move_pointer(x, y, 0);
}
ORIGINAL_IN("storm", 0x2a6ea, aim_to_screen);

// $2a6a0: the cursor and the aim back to weapon w's point.
void load_aim(int16_t w) {
  const AimPoint& p = battle.aim_points[w];
  sight.aim_map_x = p.map_x;
  sight.aim_map_y = p.map_y;
  aim_to_screen();
  sight.aim_range = battle.aim_points[w].range;
  sight.aim_bearing = battle.aim_points[w].bearing;
}
ORIGINAL_IN("storm", 0x2a6a0, load_aim);

// $297da: aiming a weapon of this type begins (unless one is being aimed):
// it needs a shallow enough depth, a free weapon slot and a loaded tube
// (TASM and Stinger come from the store). The status line shows FIRE /
// the weapon / BEARING / RANGE, and the aim starts at the view's centre.
void start_aiming(int16_t type) {
  if (battle.aiming) return;
  const Ship& own = ships[0];
  if ((type != kWireGuided && own.depth > kMaxMissileDepth) || (type == kStinger && own.depth > kPeriscopeDepth - 1)) {
    status_message(text_too_deep_launch.str(), kPenWhite, 1);
    return;
  }
  int16_t slot = 0;
  while (slot < kObjects && !(!objects[slot].state && !objects[slot].flags && objects[slot].kind == kPlayerWeapon)) ++slot;
  if (slot >= kObjects) {
    status_message(text_all_active.str(), kPenWhite, 1);
    return;
  }
  if (type == kTasm || type == kStinger) {
    if (!battle.weapon_counts[type]) {
      status_message(text_no_weapon.str(), kPenWhite, 1);
      return;
    }
    battle.fire_tube = -1;
  } else {
    int16_t tube = 0;
    while (tube < battle.tube_count && battle.tube_contents[tube] != type + kTubeLoaded) ++tube;
    if (tube >= battle.tube_count) {
      string_copy(texts().text_buffer, text_no.str());
      string_append(texts().text_buffer, const_text(weapon_names[type]));
      string_append(texts().text_buffer, text_loaded_sir.str());
      status_message(texts().text_buffer, kPenWhite, 1);
      return;
    }
    battle.fire_tube = tube;
  }
  input::flush_keys();
  battle.fired_weapon = type;
  battle.fire_slot = slot;
  battle.aiming = 1;
  screen::fill_rect_colour(kStatusLeft, kStatusTop, kStatusRight, kStatusBottom, kPenBlack);
  text_shadow(text_fire.str(), 75, kStatusTextY, kPenRed);
  text_shadow(const_text(weapon_names[battle.fired_weapon]), 104, kStatusTextY, kPenRed);
  text_shadow(text_bearing_word.str(), 160, kStatusTextY, kPenRed);
  text_shadow(text_range_word.str(), 234, kStatusTextY, kPenRed);
  texts().last_status[0] = 0;
  AimPoint& p = battle.aim_points[battle.fire_slot];
  p.map_x = battle.view_x;
  p.map_y = battle.view_y;
  p.screen_x = 185;
  p.screen_y = 94;
  p.range = 0;
  p.bearing = 0;
  battle.active_weapon = battle.fire_slot;
  load_aim(battle.fire_slot);
  battle.aim_held = 0;
}
ORIGINAL_IN("storm", 0x297da, start_aiming);

// $29a04: the aiming cursor on the map: put where the map was clicked
// (kept on the map), or moved by the keys (faster when held). While a
// weapon is aimed its bearing and range from the submarine are shown;
// for a wire-guided torpedo the cursor is the point it runs to. Fire
// (bit 4) aims the weapon there; returns 1 then, else 0.
int16_t aim_cursor() {
  const int16_t weapon = battle.active_weapon;
  const int16_t last_x = sight.aim_x, last_y = sight.aim_y;
  int16_t step = 1;
  if (ui::state().button_event) {
    ui::state().button_event = 0;
    sight.aim_x = ui::state().mouse_pos.x;
    sight.aim_y = ui::state().mouse_pos.y;
    battle.aim_held = 0;
    const int16_t x = common::clamp(sight.aim_x, kMapLeft, kMapRight);
    const int16_t y = common::clamp(sight.aim_y, kMapTop, kMapBottom);
    if (x != sight.aim_x || y != sight.aim_y) {
      sight.aim_x = x;
      sight.aim_y = y;
      move_pointer(x, y, 0);
    }
  } else {
    if (battle.steer_keys == 0 || last_x > 291 || last_x < 78 || last_y > 154 || last_y < 32) battle.aim_held = 0;
    if (battle.aim_held > 4) step = 2;
    if (battle.aim_held > 8) step = 4;
    const uint8_t keys = uint8_t(battle.steer_keys);
    if (keys & input::kRight) {
      battle.aim_held += 1;
      if (last_x < kMapRight) sight.aim_x += step;
    }
    if (keys & input::kLeft) {
      battle.aim_held += 1;
      if (last_x > kMapLeft) sight.aim_x -= step;
    }
    if (keys & input::kDown) {
      battle.aim_held += 1;
      if (last_y < kMapBottom) sight.aim_y += step;
    }
    if (keys & input::kUp) {
      battle.aim_held += 1;
      if (last_y > kMapTop) sight.aim_y -= step;
    }
  }
  const int16_t x = sight.aim_x, y = sight.aim_y;
  auto map_point = [&] {
    sight.aim_map_x = int16_t(int16_t(int16_t(x - kMapCentreX) << battle.scale) + battle.view_x);
    sight.aim_map_y = int16_t(int16_t(int16_t(y - kMapCentreY) << battle.scale) + battle.view_y);
  };
  if (battle.aiming) {
    if (battle.redraw_setup != 1) {
      map_point();
      course_to(ships[0].x, ships[0].y, sight.aim_map_x, sight.aim_map_y);
      sight.aim_range = sight.course_range;
      sight.aim_bearing = sight.course_bearing;
      constexpr int16_t kBearingX = 208, kRangeX = 263, kTextY = 177, kColour = kPenRed;
      screen::draw_to(*front_bitmap);
      screen::fill_rect_colour(kBearingX, kStatusTop, 228, kStatusBottom, kPenBlack);
      text_on_bitmap1(common::format_degrees(sight.course_bearing), kBearingX, kTextY, kColour, kPenBlack);
      screen::fill_rect_colour(kRangeX, kStatusTop, kStatusRight, kStatusBottom, kPenBlack);
      text_on_bitmap1(common::number_field(int32_t(sight.course_range) * 4, 6), kRangeX, kTextY, kColour, kPenBlack);
      screen::draw_to(*back_bitmap);
      screen::blit_bitmaps(*front_bitmap, kBearingX, kStatusTop, *back_bitmap, kBearingX, kStatusTop, 20, 8);
      screen::blit_bitmaps(*front_bitmap, kRangeX, kStatusTop, *back_bitmap, kRangeX, kStatusTop, 42, 8);
    }
  } else {
    const Object& o = objects[weapon];
    if (o.seek_time > o.time || o.weapon != kWireGuided) return 0;
  }
  if (!(uint8_t(battle.steer_keys) & input::kFire)) return 0;
  if (battle.aiming) {
    battle.fire_course = sight.aim_bearing;
    battle.fire_range = sight.aim_range;
    save_aim(battle.active_weapon);
    return 1;
  }
  map_point();
  AimPoint& p = battle.aim_points[weapon];
  p.screen_x = sight.aim_x;
  p.screen_y = sight.aim_y;
  p.map_x = sight.aim_map_x;
  p.map_y = sight.aim_map_y;
  Object& o = objects[weapon];
  course_to(o.x, o.y, sight.aim_map_x, sight.aim_map_y);
  o.course = sight.course_bearing;
  o.seek_time = int16_t(o.time - int16_t(int32_t(sight.course_range) * 8 / o.speed_set));
  battle.wire_aimed = 1;
  return 1;
}
ORIGINAL_IN("storm", 0x29a04, aim_cursor);

// $25c5c: ship n's turn this step (the rudder times its speed, or
// towards an ordered course) and its depth moving to the ordered depth.
// A hard turn makes a noise (launch(3, 0)). Returns the turn.
int32_t rudder_step(int16_t n) {
  Ship& s = ships[n];
  const int16_t old_depth = s.depth, old_set = s.depth_set;
  int32_t turn = int32_t(battle.rudder) * s.speed;
  if (common::abs_saturated(int16_t(turn)) > (battle.own_class == 11 ? 85 : 70)) launch(3, 0);
  if (s.course) turn = int32_t(int16_t(common::sign(int16_t(s.course - s.heading)) * 2)) * s.speed;
  if (s.depth_set) {
    const int16_t shift = int16_t(3 - battle.time_shift);
    if (common::abs_saturated(int16_t(s.depth - s.depth_set)) <= int16_t(1 << shift)) {
      s.depth = s.depth_set;
      s.depth_set = 0;
    } else {
      s.depth = int16_t(s.depth + (common::sign(int16_t(s.depth_set - s.depth)) << shift));
    }
  }
  if (old_depth != s.depth) battle.redraw_own_ship = 1;
  if (int16_t(turn) != 0 || old_set != s.depth_set) battle.redraw_helm = 1;
  return turn;
}
ORIGINAL_IN("storm", 0x25c5c, rudder_step);

// $25984: the player's submarine against the sea bed (depth mode 0) or
// the ice (mode 1): beyond it, a message and pushed back 8 feet, the
// orders cancelled (and every eighth frame a crash, damage and the
// events); near it (within 100 feet), keep_highest(17 or 16, 0).
void depth_limits() {
  Ship& own = ships[0];
  int16_t hit = 0;
  if (battle.depth_mode == 0) {
    const int16_t bottom = depth_at(own.x, own.y);
    if (own.depth > bottom) {
      status_message(text_aground.str(), kPenRed, 1);
      hit = kAground;
      own.depth = int16_t(own.depth - 8);
    } else if (int16_t(bottom - own.depth) < kNearBottom) {
      keep_highest(17, 0);
    }
  }
  if (battle.depth_mode == 1) {
    const int16_t ice = depth_at(own.x, own.y);
    if (own.depth < ice) {
      status_message(text_iceberg.str(), kPenRed, 1);
      hit = kIceberg;
      own.depth = int16_t(own.depth + 8);
    } else if (int16_t(own.depth - ice) < kNearBottom) {
      keep_highest(16, 0);
    }
  }
  if (!hit) return;
  own.depth_set = own.depth;
  own.speed_set = 0;
  battle.redraw_own_ship = 1;
  battle.redraw_helm = 1;
  if (battle.frame_count & 7) return;
  audio::play_effect(kEffectHullCreak);
  random_damage();
  report_events();
  if (own.events & kEventSunk) battle.damage_word = uint16_t(battle.damage_word | uint8_t(hit));  // its low byte
}
ORIGINAL_IN("storm", 0x25984, depth_limits);

// $25674: one step of the battle: now and then the alerts and the sea's
// changes; the tubes, projectiles, ships (each eighth step in turn
// listens, detects and decides) and objects.
void battle_step() {
  if (!(battle.frame_count & 63)) {
    battle.layer_depth = common::clamp(int16_t(battle.layer_depth + common::rnd(20) - 10), kMinLayerDepth, kMaxLayerDepth);
    if (battle.selected_ship == -1) {
      keep_highest(1, 0);
      if (battle.depth_mode > 1 && area_level(ships[0].x, ships[0].y) < 0) keep_highest(2, 0);
      if (ships[0].speed > 12) keep_highest(4, 0);
    }
    attack_check();
    roll_highest();
  }
  if (!(battle.frame_count & 7)) roll_highest();
  reload_tick();
  battle.time_shift = 1;
  battle.contact_flags &= uint8_t(~kShipClose);
  move_projectiles();
  for (int16_t i = 0; i < kShips; ++i) {
    Ship& s = ships[i];
    if (!(s.flags & kAfloat)) continue;
    const int16_t turn = int16_t(s.side == 0 ? rudder_step(i) : trim_step(i));
    move_ship(i, turn);
    if (i == 0) depth_limits();
    if ((i - battle.frame_count) & 7) continue;
    if (i == 0) {
      if (ships[0].depth > 55 && (ships[0].flags & kMastRaised)) ships[0].flags &= uint8_t(~kMastRaised);
      battle.noise_floor = common::clamp(int16_t(battle.noise_floor + ((common::rnd(5) - 2) << (1 - battle.time_shift))), 0, 75);
    }
    s.flags &= uint8_t(~kNoisy);
    if (s.side == 0) ship_step(i);
    if (s.side == 1) {
      track_ship(i);
      hear(0, i);
      detect(0, i);
      hear(i, 0);
      detect(i, 0);
      ship_tactics(i);
    }
  }
  for (int16_t i = 0; i < kObjects; ++i) {
    Object& o = objects[i];
    if (!o.state && !o.flags) continue;
    object_step(i);
    o.time = int16_t(o.time - (4 >> battle.time_shift));
    if (o.time <= 0) remove_object(i);
    else if (o.course) approach(i);
    else if (o.seek_time > o.time) object_seek(i);
    if (((i - battle.frame_count) & 7) && o.depth != -1) continue;
    if (!o.state && !o.flags) continue;
    if (o.kind == kPlayerWeapon) player_weapon_step(i);
    if (o.kind == kEnemyWeapon) enemy_weapon_step(i);
  }
  battle.cycle8 = int16_t((battle.cycle8 + 1) % 8);
  battle.object_cycle = int16_t((battle.object_cycle + 1) % kObjects);
  if (!(battle.frame_count & 7)) log_all();
}
ORIGINAL_IN("storm", 0x25674, battle_step);

// $260cc: ship n moves this step: it turns (taking an ordered course once
// within the turn), enemies keep their set speed within their maximum
// (halved when damaged), the speed moves towards the set one (the
// player's slowed by turning), cavitation (the player: advice 12 with
// how fast, except class 11), the towed array follows the heading
// (deployed while straight, at speed, in sea-bed waters), the noise
// halves on a steady course, and the position advances (slow ships every
// other frame). An enemy can not go below the ice; the original also
// compares its depth with the sea bed but drops the result.
void move_ship(int16_t n, int16_t turn) {
  Ship& s = ships[n];
  const uint8_t old_events = s.events, old_flags = s.flags;
  const int16_t old_speed = s.speed, old_heading = s.heading, old_course = s.course;
  const int16_t shift = int16_t(5 - battle.time_shift);
  const int16_t off = common::abs_saturated(int16_t(s.heading - s.course));
  if (s.course && off < common::abs_saturated(int16_t(turn << shift))) {
    s.heading = s.course;
    s.course = 0;
    turn = 0;
  }
  s.heading = int16_t(s.heading + int16_t(turn << shift));
  const ShipType& t = ship_types[s.id];
  if (t.kind == kAircraftKind) turn = 0;
  if (n != 0) {
    turn = 0;
    const int16_t max = (s.flags & kDamaged) ? int16_t(t.max_speed / 2) : int16_t(t.max_speed);
    s.speed_set = common::clamp(s.speed_set, 0, max);
  }
  const int16_t target = int16_t(s.speed_set - common::abs_saturated(int16_t(turn >> 3)));
  s.speed = int16_t(s.speed + common::sign(int16_t(target - s.speed)));
  if (s.depth < int16_t(45 * (s.speed - 13))) {
    s.flags |= kCavitating;
    if (n == 0) {
      if (ships[0].id == kOwnClassQuiet) s.flags &= uint8_t(~kCavitating);
      else keep_highest(12, int16_t(int16_t(s.speed - 13 + 45) / 100 + 1));
    }
  } else {
    s.flags &= uint8_t(~kCavitating);
  }
  s.array_heading = int16_t(s.array_heading + (common::sign(int16_t(s.heading - s.array_heading)) << (9 - battle.time_shift)));
  if (s.speed == 0 || battle.depth_mode == 0) {
    s.flags &= uint8_t(~kTowedArray);
  } else if (common::abs_saturated(int16_t(s.array_heading - s.heading)) < kArrayStraight) {
    s.flags |= kTowedArray;
  } else if (n == 0) {
    s.flags &= uint8_t(~kTowedArray);
    keep_highest(3, 0);
  }
  if (!((battle.frame_count - 8) & 127)) {
    if (common::abs_saturated(int16_t(s.heading - s.last_heading)) < kSteadyTurn) s.noise = int16_t(s.noise / 2);
    else if (n == 0) keep_highest(11, 0);
    s.last_heading = s.heading;
  }
  if (s.speed > 11) {
    const int16_t odd = int16_t(battle.frame_count & 1);
    s.x = int16_t(s.x + (int16_t(int16_t(common::sin_mul(s.heading, s.speed) >> battle.time_shift) + odd) >> 1));
    s.y = int16_t(s.y - (int16_t(int16_t(common::cos_mul(s.heading, s.speed) >> battle.time_shift) + odd) >> 1));
  } else if (battle.frame_count & 1) {
    s.x = int16_t(s.x + (common::sin_mul(s.heading, s.speed) >> battle.time_shift));
    s.y = int16_t(s.y - (common::cos_mul(s.heading, s.speed) >> battle.time_shift));
  }
  if (n == 0) {
    if (s.events != old_events || s.flags != old_flags || s.speed != old_speed || s.heading != old_heading ||
        s.course != old_course)
      battle.redraw_own_ship = 1;
    return;
  }
  s.flags &= uint8_t(~kDetected);
  if (battle.depth_mode == 1) {
    const int16_t ice = depth_at(s.x, s.y);
    if (s.depth < ice) s.depth = ice;
  }
}
ORIGINAL_IN("storm", 0x260cc, move_ship);

// $27c74: what the listener's sensors make of the target: scores for the
// active sonar, the hull array and the towed array (unless damaged, for
// the player); the best of them (the active sonar when it is on and
// loud enough, which also makes the listener loud, and an enemy's ping is
// announced), or sight at periscope depth. The signals relative to the
// background noise go to the target (the player listening) or the
// listener; the player's advice by what is heard.
void hear(int16_t l, int16_t t) {
  Ship& L = ships[l];
  Ship& T = ships[t];
  const bool player = l == 0;
  const int16_t brg = player ? int16_t(T.bearing + kHalfTurn) : int16_t(L.bearing);
  const int16_t rng = player ? T.range : L.range;
  int16_t active = attack_score(body(L), body(T), 0, brg, rng, T.noise ? int16_t(4) : int16_t(0));
  int16_t hull = player && (battle.damage_word & kDamagePassiveSonar) ? int16_t(0) : attack_score(body(L), body(T), 1, brg, rng, 0);
  int16_t towed = player && (battle.damage_word & kDamageTowedArray) ? int16_t(0) : attack_score(body(L), body(T), 2, brg, rng, 0);
  int16_t sensor = 1, best = hull;
  if ((L.flags & kTowedArray) && !(battle.damage_word & kDamageTowedArray) && towed > hull) {
    sensor = 2;
    best = towed;
  }
  if ((L.flags & kDetected) && (active > best || active > battle.noise_floor)) {
    sensor = 0;
    best = active;
    if (active > battle.noise_floor) T.noise = 255;
    if (L.id != 0) {
      L.noise = common::clamp(L.noise, 32, 255);
      if (player) {
        T.noise = int16_t(T.noise | 0x10);
      } else {
        if (rng < kNearRange) battle.quiet_frames = 0;
        L.flags_7 |= kPlayerContact;
        if (active > battle.noise_floor) {
          status_bearing(text_active_sonar.str(), int16_t(brg + kHalfTurn), kPenYellow);
          const int16_t g = ship_types[L.id].guidance[0];
          if (g >= 0 && g <= 2) audio::play_effect(kEffectPings[g]);
        }
      }
    }
  }
  if (T.depth < 1 && (rng >> 8) < 60 - L.depth) {
    sensor = 3;
    best = int16_t(battle.noise_floor + 32);
    if (L.flags & kMastRaised) T.noise = 255;
  }
  auto relative = [](int16_t v) { return common::clamp(int16_t(v - battle.noise_floor), -kSignalLimit, kSignalLimit); };
  best = relative(best);
  active = relative(active);
  hull = relative(hull);
  towed = relative(towed);
  if ((player && (T.flags_7 & kPlayerContact)) || (L.flags_7 & kTracksPlayer)) best = int16_t(best + 16);
  if (!player) {
    L.sonar_signal = best;
    L.hears_by = sensor;
    L.gauge_46 = active;
    L.gauge_48 = hull;
    L.gauge_50 = towed;
    return;
  }
  T.heard_signal = best;
  T.heard_by = sensor;
  T.gauge_active = active;
  T.gauge_hull = hull;
  T.gauge_towed = towed;
  const int16_t where = int16_t(T.bearing + kHalfTurn);
  if (T.noise < 64) {
    if (T.noise) keep_highest(6, where);
  } else if (!(T.flags_7 & 3)) {
    keep_highest(6, where);
  } else if (T.depth > 1) {
    keep_highest(T.range < kCloseContact || battle.era < 2 ? int16_t(7) : int16_t(8), where);
  } else {
    keep_highest(T.range < kCloseContact ? int16_t(9) : int16_t(10), where);
  }
}
ORIGINAL_IN("storm", 0x27c74, hear);

// $28552: enemy ship n's tactics this step. On the surface it may see the
// submarine (closer, or the mast up: the submarine is noted and gets
// louder). Unaware of the submarine, ships follow the group's route
// (escorts screen a group ship); aware, they head for it (or where it
// was last heard, blurred by the setting), may drop RBUs, fire when it
// is loud enough (a transient is announced), turn away when weak, and
// aircraft drop sonobuoys and eventually leave. The set speed is then
// scaled to the type's maximum.
void ship_tactics(int16_t n) {
  Ship& s = ships[n];
  Ship& own = ships[0];
  const ShipType& t = ship_types[s.id];
  course_to(s.x, s.y, own.x, own.y);
  if (s.depth < 1) {
    const int16_t visible = int16_t((60 - own.depth) - (s.range >> 8));
    if (visible > 0) {
      keep_highest(15, visible);
      if (!common::rnd(int16_t(80 - visible + int16_t(s.depth << 5)))) {
        s.flags_7 |= kTracksPlayer;
        own.noise = 255;
        sight.launch_x = own.x;
        sight.launch_y = own.y;
      }
      if (own.flags & kMastRaised) {
        s.flags_7 |= kTracksPlayer;
        own.noise = common::clamp(int16_t(own.noise + 16), 64, 255);
      }
    }
  }
  const BattleStart& route = *battle_start();
  if ((s.flags & kAlerted) || (s.heard & 1)) {
    s.speed_set = 6;
  } else {
    if (own.noise == 0) {
      if (t.kind != kAircraftKind) {
        s.course = contact_bearings[route.route_direction];
      } else if (!(battle.frame_count & 120)) {
        int16_t k;
        do {
          k = int16_t(common::rnd(5) + 1);
          const Ship& lead = ships[k];
          if (!(lead.flags & kAfloat)) continue;
          const int16_t dir = contact_bearings[route.route_direction];
          const int16_t ahead = int16_t(lead.speed << 8);
          const int16_t tx = int16_t(lead.x + common::sin_mul(dir, ahead));
          const int16_t ty = int16_t(lead.y - common::cos_mul(dir, ahead));
          s.course = common::bearing(int16_t(tx - s.x), int16_t(-(ty - s.y)));
        } while (!(ships[k].flags & kAfloat));
      }
      s.speed_set = int16_t(((battle.enemy_tactics & 12) >> 2) + 1);
    } else {
      if (s.heard || s.flags_7) {
        s.course = s.bearing;
      } else {
        const int16_t b = common::bearing(int16_t(sight.launch_x - s.x), int16_t(-(sight.launch_y - s.y)));
        s.course = int16_t(b + int16_t(int16_t((common::rnd(16) - 8) << 8) / (battle.difficulty + 1)));
      }
      s.speed_set = t.kind == kSubmarineKind ? int16_t(1) : int16_t(3);
      if (s.depth == 0 && (t.weapons & kHasRbu) && (battle.frame_count & 56) + own.noise >= kRbuNoise &&
          common::rnd(int16_t(battle.difficulty << 9)) > sight.course_range)
        rbu_attack();
      const int16_t threshold = int16_t(255 - (((battle.enemy_tactics & 3) + ((s.flags_7 & kTracksPlayer) >> 2)) << 5));
      if (own.noise > threshold) {
        enemy_launch(n, s.bearing, s.range, own.noise);
        if ((s.flags & kNoisy) && t.kind == kSubmarineKind) {
          status_bearing(text_transient.str(), int16_t(s.bearing + kHalfTurn), kPenYellow);
          audio::play_effect(kEffectTransient);
        }
        if ((s.flags & kNoisy) && t.kind == kAircraftKind) {
          battle.aircraft_attacks += 1;
          if (battle.aircraft_attacks >= battle.difficulty + 3) {
            s.events = 0;
            s.flags = 0;
          }
        }
      }
      if (t.toughness > 80 || (!t.unknown_30 && !t.weapons)) {
        s.course = int16_t(s.course ^ kHalfTurn);
        s.speed_set = 4;
      }
    }
    if (t.kind == kAircraftKind) {
      s.speed_set = 6;
      if (!(battle.frame_count & 56) && s.range < kSonobuoyRange) {
        launch(5, n);
        status_bearing(text_sonobuoy2.str(), int16_t(s.bearing + kHalfTurn), kPenYellow);
      }
    }
    if ((((battle.frame_count >> 5) + 8) & 15) == n) s.speed_set = 0;
  }
  s.speed_set = int16_t(int16_t(s.speed_set * t.max_speed) / 6 + (battle.layer_depth & 3));
}
ORIGINAL_IN("storm", 0x28552, ship_tactics);

// $2781c: enemy weapon i's step: the submarine hears it (it gets louder);
// a missed torpedo far away with little time left gives up. Once
// seeking, it homes on the submarine when its score is high enough, else
// a noisemaker or decoy in front of it can seduce it (the nearest one,
// or a noisemaker at once); the advice and messages follow.
void enemy_weapon_step(int16_t i) {
  Object& o = objects[i];
  Ship& own = ships[0];
  course_to(o.x, o.y, own.x, own.y);
  o.bearing = sight.course_bearing;
  o.range = sight.course_range;
  const int16_t heard = attack_score(body(own), body(o), 1, o.bearing, o.range, 0);
  if (heard > battle.noise_floor) o.noise = common::clamp(int16_t(o.noise + heard - battle.noise_floor), 0, 255);
  if (o.depth < 0) return;
  if (o.seek_time > o.time && sight.course_range > kGiveUpRange && o.time < kGiveUpTime &&
      (common::abs_saturated(int16_t(sight.course_bearing - o.heading)) >> 8) > 64)
    remove_object(i);
  if (o.seek_time < o.time) return;
  const int16_t was = o.homing;
  o.homing = 0;
  const int16_t score =
      attack_score(body(o), body(own), 0, o.bearing, o.range, int16_t(battle.difficulty * 2));
  int16_t lock = kNoLock;
  if (score > kLockScore) {
    o.homing = 1;
    o.steered = 3;
    lock = sight.course_range;
    o.course = sight.course_bearing ? int16_t(sight.course_bearing) : int16_t(1);
  }
  for (int16_t k = 0; k < kProjectiles; ++k) {
    const Projectile& p = projectiles[k];
    if (!p.time || (p.kind != kNoisemaker && p.kind != kDecoy && p.kind != kKnuckle)) continue;
    course_to(o.x, o.y, p.x, p.y);
    if ((common::abs_saturated(int16_t(o.heading - sight.course_bearing)) >> 8) >= 24) continue;
    if (own.speed < battle.difficulty * 3) continue;
    if (sight.course_range < 512) o.steered = 3;
    if (p.kind == kNoisemaker) {
      o.homing = -3;
      o.course = sight.course_bearing;
      o.bearing = sight.course_bearing;
      break;
    }
    if (sight.course_range < lock) {
      o.homing = -1;
      o.course = sight.course_bearing;
      o.bearing = sight.course_bearing;
      lock = sight.course_range;
      break;
    }
  }
  const int16_t where = int16_t(o.bearing + kHalfTurn);
  if (o.homing != 1) {
    keep_highest(14, where);
    if (was == 1) status_message(text_torpedo_decoyed.str(), kPenYellow, 1);
    return;
  }
  if (battle.difficulty > 1 && o.time < 255) {
    o.time = 255;
    o.seek_time = 255;
  }
  status_message(text_torpedo_approaching.str(), kPenRed, 1);
  own.noise = common::clamp(int16_t(own.noise + 16), 0, 255);
  audio::set_tone(int16_t(o.range >> 3));
  audio::play_effect(kEffectTorpedoApproaching);
  keep_highest(o.range < 128 ? int16_t(19) : int16_t(18), where);
}
ORIGINAL_IN("storm", 0x2781c, enemy_weapon_step);

// $268ca: weapon i once it seeks. A torpedo runs down its time at its
// type's speed; a missile reaching the water becomes a torpedo (under the
// ice or in shallows it may be lost; an enemy one is announced). A
// seduced one weaves; a homing one turns onto its target, follows its
// depth, and close enough hits it (an aircraft only by weapon 6, ships
// only by others): damage or sinking, the event's picture and a message.
// Otherwise it searches (snaking or circling).
void object_seek(int16_t i) {
  Object& o = objects[i];
  const int16_t w = int16_t(o.weapon);
  if (w == kMark48 || w == kSeaLance || w == kEnemyTorpedo || w == kEnemyMissile) {
    o.time = int16_t(o.time - (4 >> battle.time_shift));
    o.speed_set = ship_types[o.id].max_speed;
    if (o.depth < 0) {
      o.depth = 1;
      o.speed = ship_types[o.id].max_speed;
      o.steered = 3;
      if (battle.depth_mode == 1 && common::rnd(int16_t((area_level(o.x, o.y) + 6) << 3)) < 24) remove_object(i);
      if (battle.depth_mode == 2 && common::rnd(int16_t((area_level(o.x, o.y) + 6) << 3)) < 8) remove_object(i);
      if (o.kind == kEnemyWeapon) {
        status_bearing(text_enemy_torpedo.str(), int16_t(o.bearing + kHalfTurn), kPenRed);
        audio::play_effect(kEffectEnemyTorpedo);
      }
    }
  }
  o.noise = 255;
  o.unknown_6 = int16_t(o.unknown_6 | 11);
  const int16_t step = int16_t(11 - battle.time_shift);
  if (o.homing == -3 && o.depth > 0 && (battle.difficulty > 1 || o.kind == kPlayerWeapon)) {
    const int16_t side = o.kind == kPlayerWeapon ? int16_t(o.flags & kTurnsRight) : int16_t(i & 2);
    o.heading = int16_t(o.heading + (common::sign(int16_t(side - 1)) << (step + 1)));
  }
  if (o.homing > 0) {
    battle.contact_flags |= kShipClose;
    const int16_t t = int16_t(o.homing - 1);
    Ship& s = ships[t];
    if ((s.flags & kAfloat) && !(s.events & kEventSunk)) {
      course_to(o.x, o.y, s.x, s.y);
      if (common::abs_saturated(int16_t(o.heading - sight.course_bearing)) < kTurnAway) {
        o.course = sight.course_bearing;
        o.heading = sight.course_bearing;
      }
      if (o.depth > 0) o.depth = s.depth;
      const ShipType& st = ship_types[s.id];
      const bool hits = (w == kStinger) == (st.kind == kAircraftKind);
      if (sight.course_range < (o.depth < 0 ? 96 : 32) && hits) {
        int16_t final = 0, unused = 0, event = 0;
        if (t == 0) {
          random_damage();
          string_copy(texts().hit_message, const_text(type_names[st.name]));
          final = (s.events & kEventSunk) ? 1 : 0;
          string_append(texts().hit_message, (final ? text_sunk_by_weapon : text_hit_by_weapon).addr());
        } else {
          hit_ship(t, w);
          string_copy(texts().hit_message, const_text(type_names[st.name]));
          if (s.events & kEventSunk) {
            string_append(texts().hit_message, text_sunk_by.str());
            string_append(texts().hit_message, const_text(weapon_names[w]));
            final = 1;
          } else if (s.events & kEventDamaged) {
            string_append(texts().hit_message, text_damaged_by2.str());
            string_append(texts().hit_message, const_text(weapon_names[w]));
            final = 0;
          }
        }
        if (st.kind == kSubmarineKind) {
          unused = 1;
          event = final ? kPictureSubmarineSunk : kPictureSubmarineHit;
        }
        if (st.kind == kSurfaceShipKind) {
          unused = 0;
          event = (w == kMark48 || w == kSeaLance) ? kPictureShipHitTorpedo : kPictureShipHitMissile;
        }
        if (st.kind == kAircraftKind) {
          battle.aircraft_shot_down = 1;
          final = unused = event = 0;
        }
        show_event(event, unused, int16_t(s.id), final);
        remove_object(i);
        status_message(texts().hit_message, t == kPenBlack ? kPenRed : kPenYellow, 1);
        if (t == 0) {
          report_events();
          if (final == 1) battle.damage_word |= kDamageSunkShown;
        }
      }
      if (o.depth >= 0) o.depth = common::clamp(int16_t(o.depth + (32 >> battle.time_shift)), kMinRunDepth, kMaxRunDepth);
      else o.heading = sight.course_bearing;
    }
  }
  if (o.homing != 0) return;
  if (o.steered == 2) {
    o.heading = int16_t(o.heading + (common::sign(int16_t((o.time & 64) - 1)) << step));
  } else if (o.steered == 3) {
    battle.contact_flags |= kShipClose;
    if (o.kind == kPlayerWeapon) {
      o.heading = int16_t(o.heading + (common::sign(int16_t((o.flags & kTurnsRight) - 1)) << step));
    } else {
      int16_t turn = int16_t(common::sign(int16_t((o.time & 512) - 1)) << step);
      if (o.kind == kEnemyWeapon) {
        o.depth = int16_t(o.depth + 4);
        if (i & 1) turn = int16_t(-turn);
      }
      o.heading = int16_t(o.heading + turn);
    }
  }
}
ORIGINAL_IN("storm", 0x268ca, object_seek);

// $2704c: the player's weapon i among the enemy ships. A missile in
// flight may be shot down by a surface ship. Searching, it picks the best
// target (a torpedo by attack score, a missile the nearest in front of
// it). Enemies that hear a torpedo note where it came from, may fire
// back and turn across its path. A jammer in front of a seeking torpedo
// decoys it; with a good target it homes (and the target may launch a
// jammer); a missile homes on a target close in front. A missile that
// found nothing in time is lost.
void player_weapon_step(int16_t i) {
  Object& o = objects[i];
  Ship& own = ships[0];
  int16_t best = o.depth < 0 ? kNoTarget : int16_t(0);
  int16_t target = 0;
  for (int16_t k = 0; k < kShips; ++k) {
    Ship& s = ships[k];
    if (!(s.flags & kAfloat) || s.side != 1) continue;
    course_to(o.x, o.y, s.x, s.y);
    const ShipType& st = ship_types[s.id];
    if (o.depth < 0 && !((i - battle.frame_count) & 7) && (o.seek_time > o.time || o.weapon == kSeaLance) && s.depth == 0) {
      const int16_t distance = int16_t(sight.course_range >> 6);
      const int16_t roll = common::rnd(
          int16_t(int16_t((battle.difficulty + 1) * st.air_defence) * ((s.flags & kDamaged) ? 4 : 8) + 192));
      if (distance + 192 < roll) {
        remove_object(i);
        string_copy(texts().hit_message, const_text(weapon_names[int16_t(o.weapon)]));
        string_append(texts().hit_message, text_shot_down2.str());
        show_event(kPictureShotDown, 0, int16_t(s.id), 0);
        status_message(texts().hit_message, kPenCyan, 1);
        return;
      }
    }
    o.homing = 0;
    const bool searching = o.seek_time > o.time;
    if (o.depth > 0 && searching) {
      const int16_t score = attack_score(body(o), body(s), 0, sight.course_bearing, sight.course_range, 0);
      if (score > best) {
        target = k;
        best = score;
      }
    }
    if (o.depth < 0 && searching) {
      const bool can = (s.depth == 0 && o.weapon != kStinger) || (s.depth < 0 && o.weapon == kStinger);
      if (can && common::abs_saturated(int16_t(o.heading - sight.course_bearing)) < kTurnAway &&
          -sight.course_range > best) {
        target = k;
        best = int16_t(-sight.course_range);
      }
    }
    if (o.depth <= 0) continue;
    int16_t heard = attack_score(body(s), body(o), (battle.frame_count & 8) ? int16_t(1) : int16_t(2),
                                 int16_t(sight.course_bearing + kHalfTurn), sight.course_range, 0);
    if (searching) heard = int16_t(heard + 50);
    if (heard <= battle.noise_floor) continue;
    if (own.noise == 0) {
      sight.launch_x = o.x;
      sight.launch_y = o.y;
    }
    own.noise = common::clamp(own.noise, 24, 255);
    if ((common::abs_saturated(int16_t(o.heading - sight.course_bearing)) >> 8) + (sight.course_range >> 4) >= 128) continue;
    if ((battle.frame_count & 48) == ((k & 3) << 4)) {
      const int16_t aim = (s.flags_7 & kTracksPlayer) ? int16_t(s.bearing) : int16_t(sight.course_bearing + kHalfTurn);
      enemy_launch(k, aim, sight.course_range, 255);
      if ((s.flags & kNoisy) && st.kind == kSubmarineKind) {
        status_bearing(text_transient2.str(), int16_t(s.bearing + kHalfTurn), kPenYellow);
        audio::play_effect(kEffectTransient);
      }
    }
    s.course = int16_t(sight.course_bearing + (common::sign(int16_t(sight.course_bearing - o.heading)) << 14));
    s.flags |= kAlerted;
  }
  if (o.seek_time > o.time) {
    if (o.depth > 0) {
      for (int16_t p = 0; p < kProjectiles; ++p) {
        const Projectile& j = projectiles[p];
        if (!j.time || j.kind != kJammer) continue;
        course_to(o.x, o.y, j.x, j.y);
        if ((common::abs_saturated(int16_t(o.heading - sight.course_bearing)) >> 8) < kDecoyed) {
          status_message(text_torpedo_decoyed2.str(), kPenCyan, 1);
          best = 0;
          target = 0;
        }
      }
    }
    if (best > 0) {
      Ship& s = ships[target];
      course_to(o.x, o.y, s.x, s.y);
      if (s.flags & kAlerted) s.course = sight.course_bearing;
      if (best > kLockScore) {
        if (o.homing <= 0) status_message(text_torpedo_homing.str(), kPenCyan, 1);
        o.homing = int16_t(target + 1);
        s.noise = 255;
        if (s.flags & kAlerted) s.flags |= kDetected;
        s.flags_7 |= kPlayerContact;
        o.steered = 3;
        const int16_t when = int16_t((battle.frame_count + 8) & 248);
        const int16_t mask = int16_t(int16_t(255 >> battle.difficulty) << (s.depth == 0 ? 1 : 0));
        const ShipType& st = ship_types[s.id];
        if (!(when & mask) && s.noise > 224 && (st.unknown_30 || st.weapons)) {
          launch(kJammer, target);
          status_message(text_noise_jamming.str(), kPenYellow, 1);
        }
      }
    }
    if (best < 0 && best > kHomingRange) {
      o.homing = int16_t(target + 1);
      ships[target].noise = 255;
    }
  }
  if (o.seek_time > o.time + 20 && o.depth < 0 && best == kNoTarget && o.weapon != kStinger) remove_object(i);
}
ORIGINAL_IN("storm", 0x2704c, player_weapon_step);

}  // namespace game::storm
