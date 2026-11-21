// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Storm (the battle module): the battle's state, kept in C++:
// the ships, the
// weapons and other objects in the water, the projectiles, the battle
// log. It replaces Storm's BSS at a4-8610..+5206 (reset when the module
// starts). Field comments keep the original's byte offsets.
#pragma once

#include "game/mem/image.hpp"
#include "game/mem/state.hpp"

#include <cstdint>

namespace game::storm {

// A ship (62 bytes; 8 of them, ship 0 is the player's submarine).
struct Ship {
  uint16_t side;         // +0: 0 the player's, 1 the enemy's
  uint16_t id;           // +2: its type (ShipType)
  uint8_t events;    // +4: kEventLaunched, kEventDamaged, kEventSunk (for the log)
  uint8_t flags;     // +5: bit 7 active, 5 detected (the player's: active sonar), 4 noisy, 2 mast
                     // raised, 1 towed array out, 0 cavitating
  uint8_t heard;     // +6: bit 0: heard since the last look
  uint8_t flags_7;   // +7: bits 0-1 how well it is identified (3: fully), 3 a contact of the player,
                     // 4 (enemy) the player is its contact / reported once by ship_step
  int16_t x, y;        // +8
  int16_t depth;       // +12
  int16_t speed;       // +14
  int16_t heading;     // +16
  int16_t mark_x, mark_y;  // +18: where the tactical display last drew it (0: not yet)
  int16_t mark_heading;    // +22
  int16_t depth_set;   // +24: the ordered depth (0: none)
  int16_t speed_set;   // +26: the ordered speed
  int16_t course;      // +28: the ordered course (0: none)
  int16_t noise;       // +30
  int16_t bearing;     // +32: from the player's submarine
  int16_t range;       // +34
  int16_t gauge_active, gauge_hull, gauge_towed;  // +36: the player's sensors on this ship (display 2)
  int16_t heard_by;    // +42: the player's sensor that hears this ship (sensor_names; 0 none)
  int16_t heard_signal;// +44
  int16_t gauge_46, gauge_48, gauge_50;           // +46: this ship's, shown once it is identified
  int16_t hears_by;    // +52: this ship's sensor that hears the player
  int16_t sonar_signal;// +54
  int16_t array_heading; // +56: the towed array's heading, following the ship's
  int16_t last_heading;  // +58: the heading 128 frames ago
  int16_t last_launch; // +60: the frame it last fired
};

// battle.display_mode (besides the tactical displays 1, 3 and 5).
inline constexpr int16_t kDisplayContact = 2, kDisplaySonar = 4, kDisplayPeriscope = 6, kDisplayLibrary = 7,
                         kDisplayIdentify = 8;
// battle.sub_mode: the panels' page.
inline constexpr int16_t kPageContacts = 1, kPageWeapons = 2, kPageDamage = 3, kPageTorpedo = 4, kPageThreats = 5,
                         kPageReloads = 9;

// Ship::flags.
inline constexpr uint8_t kCavitating = 0x01;  // (also an object's)
inline constexpr uint8_t kTowedArray = 0x02;  // the towed array is out
inline constexpr uint8_t kMastRaised = 0x04;  // the player's: the radar is on
inline constexpr uint8_t kAlerted = 0x08;     // it heard the player (it steers towards the bearing)
inline constexpr uint8_t kNoisy = 0x10;       // making noise (it fired; the player: not the quiet class)
inline constexpr uint8_t kDetected = 0x20;    // the player's: the active sonar is on
inline constexpr uint8_t kDamaged = 0x40;     // half speed, easier to hit
inline constexpr uint8_t kAfloat = 0x80;
// Ship::flags_7.
inline constexpr uint8_t kIdentified = 0x03;     // how well it is identified (3: fully)
inline constexpr uint8_t kPlayerContact = 0x08;  // a contact of the player
inline constexpr uint8_t kTracksPlayer = 0x10;   // (an enemy) the player is its contact
// battle.contact_flags.
inline constexpr uint8_t kDeepSea = 0x08, kUnderIce = 0x10;  // under ice the enemy's torpedoes reach further
inline constexpr uint8_t kNewContact = 0x20, kShipClose = 0x40;
// Ship::events (for the log).
inline constexpr uint8_t kEventLaunched = 0x02, kEventDamaged = 0x04, kEventSunk = 0x08;

// An object: a contact, weapon or decoy (48 bytes; 14 of them).
struct Object {
  uint16_t kind;         // +0: kPlayerWeapon (can be selected), kEnemyWeapon
  uint16_t id;           // +2
  uint8_t state;     // +4: with flags, nonzero while it exists
  uint8_t flags;     // +5: kRunning, kWired, kTurnsRight, kCavitating
  int16_t unknown_6;
  int16_t x, y;        // +8
  int16_t depth;       // +12: -1 on the surface (stepped every step)
  int16_t speed;       // +14
  int16_t heading;     // +16
  int16_t mark_x, mark_y;  // +18: where the tactical display last drew it (0: not yet)
  int16_t mark_heading;    // +22
  uint8_t unknown_24[2];
  int16_t speed_set;   // +26: the speed it changes to
  int16_t course;      // +28: the course it turns to (0: none)
  int16_t noise;       // +30
  int16_t bearing, range;  // +32: an enemy weapon's from the submarine
  int16_t tube;        // +36: fired from (-1: none)
  uint16_t weapon;       // +38: its weapon type (1 a wire-guided torpedo; 1 and 2 can be selected)
  uint16_t steered;      // +40: steered by the player
  int16_t time;        // +42: runs down; at 0 the object is removed
  int16_t seek_time;   // +44: below this time it seeks (and can be steered)
  int16_t homing;      // +46: > 0 homing on a target, < 0 jammed, 0 searching
};

// Object::kind and Object::flags.
inline constexpr uint16_t kPlayerWeapon = 2, kEnemyWeapon = 3;
inline constexpr uint8_t kTurnsRight = 0x04;  // a wire-guided torpedo searches to the right
inline constexpr uint8_t kWired = 0x20;       // still on its wire
inline constexpr uint8_t kRunning = 0x80;

// An entry of the battle log (6 bytes; 2026 fit).
struct LogEntry {
  uint16_t what;         // ship 0 $100, ships $200, objects 0-3 $400, others $800; | the id; bits 12-14 events
  int16_t x, y;
};

// A projectile (sonobuoys, decoys ...; formerly 26 bytes each). Its side,
// flags and speed are where a ship's are (body() reads them); nothing
// sets them.
struct Projectile {
  int16_t side;        // +0 (0)
  uint16_t kind;         // +2: 2 moves
  uint8_t unknown_4;
  uint8_t flags;       // +5 (0)
  uint8_t unknown_6[2];
  int16_t x, y;        // +8
  int16_t depth;       // +12
  int16_t speed;       // +14 (0)
  int16_t heading;     // +16
  uint8_t unknown_18[6];
  int16_t time;        // +24: 0 unused
};

inline constexpr int kLogCapacity = 2047;  // kLogEntries

// The aiming, the periscope and the sea (formerly words in Storm's BSS).
struct Tactical {
  int16_t previous_nearest_range;   // a4-9296
  int16_t aim_map_x;                // a4-7118
  int16_t aim_map_y;                // a4-7116
  int16_t aim_x;                    // a4-7114
  int16_t aim_y;                    // a4-7112
  int16_t aim_range;                // a4-7110
  int16_t aim_bearing;              // a4-7108
  int16_t launch_x;                 // a4-7102: where the enemy heard a launch
  int16_t launch_y;                 // a4-7100: where the enemy heard a launch
  int16_t course_range;             // a4-7098
  int16_t course_bearing;           // a4-7096
  int16_t periscope_bearing;        // a4-7094
  uint16_t periscope_phase;         // a4-7092: bit 0 toggles each view: the ships' images change every other one
  int16_t ship_image;          // a4-7090: the image (0-7, by aspect) of the ship in view
  int16_t sea_type;                 // a4-7088
  int16_t wave_x;              // a4-7086: the periscope's wave
  int16_t wave_y;              // a4-7084
  int16_t seen_type;           // a4-7082: the type of the ship under the reticle (-1: none)
  int16_t pending_effect;      // a4-7080: sound effect played in sub-mode 1 (0: none)
  uint16_t blink;              // a45240: 0/1, flips each tactical frame
  uint16_t question;           // a45244: the start message's question (index in start_messages)
};

struct State {
  Ship ships[8];              // a4-8610: ship 0 is the player's submarine
  uint16_t ship_list[32];     // a4-8114: the ship types in the identification list (kShipList used)
  Projectile projectiles[10]; // a4-8050
  Object objects[14];         // a4-7790
  LogEntry log[kLogCapacity]; // a4-7076
  Tactical tactical;
  int16_t shade_pens[32];     // a4-8674: the sonar shades' pens
};

inline State state;
inline Ship (&ships)[8] = state.ships;
inline uint16_t (&ship_list)[32] = state.ship_list;
inline Projectile (&projectiles)[10] = state.projectiles;
inline Object (&objects)[14] = state.objects;
inline LogEntry (&battle_log)[kLogCapacity] = state.log;
inline Tactical& sight = state.tactical;
inline int16_t (&shade_pens)[32] = state.shade_pens;

// The a4 ranges of Storm's small-data area that State replaces.
inline constexpr mem::state::Range kMovedRanges[] = {
    {-8610, 1492},               // ships, ship_list, projectiles, objects
    {-7076, kLogCapacity * 6},   // the battle log
    {-8674, 64},  // shade_pens
    {-7118, 12}, {-7102, 24}, {-9296, 2}, {5240, 2}, {5244, 2},  // tactical
};

// Where a weapon was aimed (per object; formerly 12 bytes each at
// a4-32700, initialised data).
struct AimPoint {
  int16_t map_x, map_y;        // +0
  int16_t screen_x, screen_y;  // +4
  int16_t range, bearing;      // +8
};
IMAGE_FIELDS(AimPoint, s.map_x, s.map_y, s.screen_x, s.screen_y, s.range, s.bearing);
// The threats panel's enemy weapons, nearest first (formerly 6 bytes
// each at a4-31224, initialised data).
struct Threat {
  int16_t object, range, bearing;
};
IMAGE_FIELDS(Threat, s.object, s.range, s.bearing);

// The battle's variables in Storm's initialised data (a4-31248..-30570):
// settings, the submarine's controls and weapons, the sea's conditions,
// the displays, the counters. Taken from the loaded image at Storm's first
// start after a load (the read-only tables between them stay in emulated
// memory).
struct Battle {
  AimPoint aim_points[8];                 // a4-32700: by the player's weapon objects (0-7)
  Threat threats[6];                      // a4-31224
  int16_t noise_half;                    // a4-31248: sea_noise's dots: 2 frames a row, 16 rows
  int16_t noise_row;                     // a4-31246: sea_noise's dots: 2 frames a row, 16 rows
  int16_t era;                           // a4-31186: 0-3, the year (the ship lists, torpedo speed)
  int16_t difficulty;                    // a4-31184: 0-3; also the most hits one blow does - 1
  uint16_t no_damage;                    // a4-31182: the own ship takes no damage (a level above 7)
  int16_t own_class;                     // a4-31180
  uint16_t action_track;                 // a4-31178: event pictures on (they bring their sounds); the keys toggle its low byte
  int16_t rudder;                        // a4-31176: -3..3
  uint16_t damage_word;                  // a4-31174: kDamage* (the low byte: own_damage)
  int16_t tube_count;                    // a4-31172
  int16_t tube_contents[8];              // a4-31170: [tube]: 0 empty
  int16_t weapon_counts[8];              // a4-31154: [weapon]
  int16_t load_times[4];                 // a4-31138: [tube]
  int16_t fire_course;                   // a4-31122
  int16_t fire_range;                    // a4-31120
  int16_t fired_weapon;                  // a4-31118: the weapon type being fired
  int16_t fire_tube;                     // a4-31116: -1: not from a tube
  int16_t fire_slot;                     // a4-31114: the object it becomes
  uint16_t aiming;                       // a4-31112: a weapon is being aimed (no status messages meanwhile)
  int16_t pending_weapon;                // a4-31110
  int16_t active_weapon;                 // a4-31108: an object; -1: none
  uint16_t wire_aimed;                   // a4-31106: the wire-guided torpedo was given a point to run to
  int16_t library_choice;                // a4-31104: the sonar library's letter chosen (-1: none)
  int16_t aim_held;                      // a4-31102: frames a direction was held
  int16_t layer_depth;                   // a4-31098: the thermal layer's depth (feet)
  int16_t ambient_noise;                 // a4-31096: shown times 3
  int16_t noise_floor;                   // a4-31094: 0-75, a random walk: a signal is heard above it
  int16_t duct_strength;                 // a4-31092: the surface duct and the thermal layer
  int16_t layer_strength;                // a4-31090: the surface duct and the thermal layer
  int16_t depth_mode;                    // a4-31088: 0: by the sea bed, 1: the table ice_depths
  int16_t scale;                         // a4-30946: 0-7
  int16_t named_ship;                    // a4-30944: the ship whose name display 2 shows
  int16_t selected_ship;                 // a4-30942: -1: none
  int16_t selected_object;               // a4-30940: -1: none
  int16_t shown_mode;                    // a4-30938
  int16_t display_mode;                  // a4-30936: 1, 3, 5 the tactical displays; kDisplay*
  int16_t saved_sub_mode;                // a4-30934
  int16_t sub_mode;                      // a4-30932: the panels' page (kPage*); kPageContacts: no engine noise
  uint16_t helm_flag;                    // a4-30930: cleared with the helm (nothing reads it)
  int16_t view_x;                        // a4-30928
  int16_t view_y;                        // a4-30926
  int16_t scales_by_mode[6];             // a4-30924
  int16_t enemy_tactics;                 // a4-30906: by difficulty: bits 0-1 the enemy's hearing and score, bits 2-3 its speed
  uint16_t redraw_setup;                 // a4-30904: the display's frame (display_setup) to be drawn
  uint16_t redraw_left;                  // a4-30902: the left panel to be drawn
  uint16_t redraw_display;               // a4-30900: cleared after the display is drawn (nothing sets it)
  uint16_t redraw_right;                 // a4-30898: a tube reloaded on the right panel's page 2 (cleared when drawn)
  uint16_t redraw_own_ship;              // a4-30896: the own ship's panel (depth) to be drawn
  uint16_t redraw_helm;                  // a4-30894: the helm panel to be drawn
  int16_t frame_count;                   // a4-30892
  uint8_t contact_flags;                 // a4-30889: kUnderIce, kDeepSea (the battle's sea), kNewContact, kShipClose: bit 6: a ship within 2000; bit 4 widens the enemy's torpedo range
  uint16_t battle_over;                  // a4-30888
  uint16_t paused;                       // a4-30886: the battle is paused (status_if_flag shows the pause message)
  int16_t time_shift;                    // a4-30878: steps are scaled by >> (0: full)
  int16_t cycle8;                        // a4-30876: 0-7, one step a frame
  int16_t object_cycle;                  // a4-30874: the object handled this frame
  int16_t last_enemy_launch;             // a4-30872: the frame an enemy last fired
  int16_t quiet_frames;                  // a4-30870: frames without a shot or a close contact (then: continue the battle?)
  uint16_t sounds_on;                    // a4-30868
  int16_t sea_bed_base;                  // a4-30866
  int16_t aircraft_attacks;              // a4-30864: an aircraft leaves after difficulty + 3 attacks
  int16_t entry_mode;                    // a4-30862: a number being typed (kEntry*; 0 none): no status messages
  int16_t typed_length;                  // a4-30860
  int16_t typed_frames;                  // a4-30858: frames since the last key of a number entry
  uint16_t fire_ready;                   // a4-30758: the selected contact can be attacked
  int16_t steer_keys;                    // a4-30756: bit 0 right, bit 1 left, bit 4 fire (from the keys)
  int16_t sea_colour;                    // a4-30754: the tactical displays' background
  int16_t time_rate;                     // a4-30752: kAcceleratedTime or 6 (real time)
  uint16_t event_flags[4];               // a4-30736: [4]: the systems damaged (report_events)
  uint16_t depth_chart_on;               // a4-30728: 1: the tactical displays show the depth chart
  int16_t highest_b;                     // a4-30726
  int16_t highest_a;                     // a4-30724
  int16_t highest_b_value;               // a4-30722
  int16_t highest_a_value;               // a4-30720
  int16_t log_count;                     // a4-30718
  int16_t log_end;                       // a4-30716
  uint16_t log_wrapped;                  // a4-30714
  int16_t log_view_index;                // a4-30710
  uint16_t aircraft_shot_down;           // a4-30708
  int16_t range_opening;                 // a4-30572: checks in a row that found the range increasing
  int16_t nearest_range;                 // a4-30570: the nearest enemy ship (0: a weapon running), every 64 frames
};
inline Battle battle;

// Battle's fields at their original offsets (load_battle, the moved ranges).
inline void load_battle() {
  auto read = [](int32_t off, auto& field) { mem::image::read(mem::state::loaded_image(off), field); };
  read(-32700, battle.aim_points);
  read(-31224, battle.threats);
  read(-31248, battle.noise_half);
  read(-31246, battle.noise_row);
  read(-31186, battle.era);
  read(-31184, battle.difficulty);
  read(-31182, battle.no_damage);
  read(-31180, battle.own_class);
  read(-31178, battle.action_track);
  read(-31176, battle.rudder);
  read(-31174, battle.damage_word);
  read(-31172, battle.tube_count);
  read(-31170, battle.tube_contents);
  read(-31154, battle.weapon_counts);
  read(-31138, battle.load_times);
  read(-31122, battle.fire_course);
  read(-31120, battle.fire_range);
  read(-31118, battle.fired_weapon);
  read(-31116, battle.fire_tube);
  read(-31114, battle.fire_slot);
  read(-31112, battle.aiming);
  read(-31110, battle.pending_weapon);
  read(-31108, battle.active_weapon);
  read(-31106, battle.wire_aimed);
  read(-31104, battle.library_choice);
  read(-31102, battle.aim_held);
  read(-31098, battle.layer_depth);
  read(-31096, battle.ambient_noise);
  read(-31094, battle.noise_floor);
  read(-31092, battle.duct_strength);
  read(-31090, battle.layer_strength);
  read(-31088, battle.depth_mode);
  read(-30946, battle.scale);
  read(-30944, battle.named_ship);
  read(-30942, battle.selected_ship);
  read(-30940, battle.selected_object);
  read(-30938, battle.shown_mode);
  read(-30936, battle.display_mode);
  read(-30934, battle.saved_sub_mode);
  read(-30932, battle.sub_mode);
  read(-30930, battle.helm_flag);
  read(-30928, battle.view_x);
  read(-30926, battle.view_y);
  read(-30924, battle.scales_by_mode);
  read(-30906, battle.enemy_tactics);
  read(-30904, battle.redraw_setup);
  read(-30902, battle.redraw_left);
  read(-30900, battle.redraw_display);
  read(-30898, battle.redraw_right);
  read(-30896, battle.redraw_own_ship);
  read(-30894, battle.redraw_helm);
  read(-30892, battle.frame_count);
  read(-30889, battle.contact_flags);
  read(-30888, battle.battle_over);
  read(-30886, battle.paused);
  read(-30878, battle.time_shift);
  read(-30876, battle.cycle8);
  read(-30874, battle.object_cycle);
  read(-30872, battle.last_enemy_launch);
  read(-30870, battle.quiet_frames);
  read(-30868, battle.sounds_on);
  read(-30866, battle.sea_bed_base);
  read(-30864, battle.aircraft_attacks);
  read(-30862, battle.entry_mode);
  read(-30860, battle.typed_length);
  read(-30858, battle.typed_frames);
  read(-30758, battle.fire_ready);
  read(-30756, battle.steer_keys);
  read(-30754, battle.sea_colour);
  read(-30752, battle.time_rate);
  read(-30736, battle.event_flags);
  read(-30728, battle.depth_chart_on);
  read(-30726, battle.highest_b);
  read(-30724, battle.highest_a);
  read(-30722, battle.highest_b_value);
  read(-30720, battle.highest_a_value);
  read(-30718, battle.log_count);
  read(-30716, battle.log_end);
  read(-30714, battle.log_wrapped);
  read(-30710, battle.log_view_index);
  read(-30708, battle.aircraft_shot_down);
  read(-30572, battle.range_opening);
  read(-30570, battle.nearest_range);
}
inline constexpr mem::state::Range kMovedData[] = {
    {-32700, 96},   // aim_points
    {-31224, 36},   // threats
    {-31248, 4},
    {-31186, 56},
    {-31122, 22},
    {-31098, 12},
    {-30946, 34},
    {-30906, 16},
    {-30889, 5},
    {-30878, 22},
    {-30758, 8},
    {-30736, 24},
    {-30710, 4},
    {-30572, 4},
};
// The fields scenario scripts set.
inline constexpr mem::state::Field kScriptFields[] = {
    {-31088, 2, &battle.depth_mode},
    {-30728, 2, &battle.depth_chart_on},
};

// What attack_score reads of an attacker or a target: the head that
// ships, objects and projectiles share in the original (it reads any of
// them as a Ship).
struct Body {
  int16_t side;     // +0 (an object's kind, a projectile's first word)
  int16_t id;       // +2 (a projectile's kind)
  uint8_t flags;    // +5
  int16_t x, y;     // +8
  int16_t depth;    // +12
  int16_t speed;    // +14
  int16_t heading;  // +16
};
inline Body body(const Ship& s) { return {int16_t(s.side), int16_t(s.id), s.flags, s.x, s.y, s.depth, s.speed, s.heading}; }
inline Body body(const Object& o) { return {int16_t(o.kind), int16_t(o.id), o.flags, o.x, o.y, o.depth, o.speed, o.heading}; }
inline Body body(const Projectile& p) {
  return {p.side, int16_t(p.kind), p.flags, p.x, p.y, p.depth, p.speed, p.heading};
}

}  // namespace game::storm
