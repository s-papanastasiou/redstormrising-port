// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red (the campaign module): the campaign's state, kept in C++.
// It moves here from
// Red's small-data area block by block; vars.hpp keeps what is still in
// the emulated memory. Reset when the module starts (it was BSS); saved
// into and taken from the player's record, which stays in emulated memory
// (it goes to Storm and to the save files).
#pragma once

#include "game/mem/image.hpp"
#include "game/mem/state.hpp"
#include "game/mem/types.hpp"

#include <cstdint>

namespace game::red {

// A date and time: the campaign clock (the year in BCD).
struct Date {
  int16_t year, month, day, hour, minute;
};
IMAGE_FIELDS(Date, s.year, s.month, s.day, s.hour, s.minute);
static_assert(mem::image::size<Date>() == 10);
// war.situation (and a group's): what the record says the war is at.
inline constexpr int16_t kNoCampaign = -1;
inline constexpr int16_t kSituationGroup0 = 10, kSituationGroup1 = 20;  // a battle with battle group 0 / 1
inline constexpr int16_t kSituationSingle = 30;                           // a single mission
inline constexpr int16_t kSituationNoBattle = 40;


// The war's phase and the battle groups' missions (the record's 50 bytes
// at +158; formerly a4+14394).
struct Phase {
  int16_t war_phase;         // +0 (a4+14394): group 0's mission, the day's tables (31: the fronts stay; -1 at a new phase)
  int16_t war_phase_b;       // +2: group 1's mission
  int16_t order_phase;       // +4 (14398): unit 0's last order: 1 started, 2 advanced, 3 a group formed
  int16_t last_order_phase;  // +6 (14400): the order phase when it last moved on (4: done)
  int16_t mission_kind;      // +8 (14402): high nibble the mission's type (kMission*), low nibble its target
  int16_t mission_needs;     // +10 (14404): low nibble units needed (enough_units), high nibble a condition
  int16_t target_a;          // +12 (14406): where unit 0 heads
  int16_t target_b;          // +14 (14408): where unit 1 heads
  uint16_t mission_names[3]; // +16 (14410): message numbers
  int16_t history[4];        // +22 (14416): the last phases, newest first
  uint16_t mask;             // +30 (14424): bit 0 per phase: it was 3-8
  Date time;                 // +32 (14426): the campaign page's date and time
  uint16_t seen_by;          // +42 (14436): bit n: unit n has the player's submarine in range
  uint16_t hunters;          // +44 (14438): bit per group
  uint16_t short_ranges;     // +46 (14440): the enemy's detection range stays 10
  uint16_t alerted;          // +48 (14442): bit per group
};
IMAGE_FIELDS(Phase, s.war_phase, s.war_phase_b, s.order_phase, s.last_order_phase, s.mission_kind, s.mission_needs,
             s.target_a, s.target_b, s.mission_names, s.history, s.mask, s.time, s.seen_by, s.hunters,
             s.short_ranges, s.alerted);
static_assert(mem::image::size<Phase>() == 50);
// The mission's type (kMission*) and target: mission_kind's nibbles.
inline int16_t mission_type(const Phase& p) { return int16_t((p.mission_kind & 0xf0) >> 4); }
inline int16_t mission_target(const Phase& p) { return int16_t(p.mission_kind & 15); }

// A battle group (24 bytes): the units in it and, for the group in a
// battle, what became of each.
struct Group {
  int16_t count;       // +0: members (-1: none formed)
  int16_t situation;   // +2: 10 for group 0, 20 for group 1 (the war situation of a battle with it)
  int16_t members[5];  // +4: unit numbers
  int16_t results[5];  // +14: in a battle: 0 unharmed, 1 hit, 2 sunk (kMemberSunk)
};
IMAGE_FIELDS(Group, s.count, s.situation, s.members, s.results);
static_assert(mem::image::size<Group>() == 24);

// The map's colours around the submarine (count_terrain).
struct Terrain {
  int16_t colour_1, colour_26, fronts, other;  // fronts: colours 2 and 9
};
IMAGE_FIELDS(Terrain, s.colour_1, s.colour_26, s.fronts, s.other);

// The war's situation and the battle about to be fought (the record's 40
// bytes at +380; formerly a4+14616).
struct War {
  int16_t situation;       // +0 (a4+14616): 0 fronts 0 and 1 fall back, else advance; kSituation*
  int16_t heading_player;  // +2: compass headings (0-15)
  int16_t heading_enemy;   // +4
  int16_t heading_unit8;   // +6
  Terrain terrain;         // +8
  Group battle;            // +16 (14632): the group in the battle (a copy)
};
IMAGE_FIELDS(War, s.situation, s.heading_player, s.heading_enemy, s.heading_unit8, s.terrain, s.battle);
static_assert(mem::image::size<War>() == 40);

// The submarine's weapons, tubes and loadout options (the record's 50
// bytes at +256; formerly a4+14492).
struct Loadout {
  int16_t weapons[7];    // +0 (a4+14492): on board, by weapon kind (-3: not listed)
  int16_t capacity;      // +14 (14506): what the magazine holds (starting_total by rank)
  int16_t on_board;      // +16 (14508): weapons of the listed kinds on board
  int16_t total_4_6;     // +18 (14510): kinds 4 and 6 together (12: full)
  int16_t player_speed;  // +20 (14512): 5 still, 15 normal, 30 fast
  int16_t unit_count;    // +22 (14514): bits set in unit_bits' low byte (enough_units)
  int16_t tube_count;    // +24 (14516): tubes in use (3, 4 or 8)
  int16_t tubes[8];      // +26 (14518): the weapon kind loaded (kTubeFree, kTubeFixed)
  uint32_t flags;        // +42 (14534): kTubes5to8, kShortList, kRow* (rows of the weapon list)
  uint16_t unit_bits;    // +46 (14538): bits 0-3 damage (2: tubes 4-8 jammed, 3: no fast speed),
                         //   4-5 the debriefing's message, 4-7 war_flag; the port repairs them (tube_masks)
  uint16_t unused_48;    // +48
};
IMAGE_FIELDS(Loadout, s.weapons, s.capacity, s.on_board, s.total_4_6, s.player_speed, s.unit_count, s.tube_count,
             s.tubes, s.flags, s.unit_bits, s.unused_48);
static_assert(mem::image::size<Loadout>() == 50);

// The day timers and the reinforcements (the record's 32 bytes at +306;
// formerly a4+14542).
struct Days {
  int16_t arrival_countdown;  // +0 (a4+14542): days to the next reinforcement
  int16_t arrival_index;      // +2 (14544): 0-4, the next reinforcement
  int16_t timers[14];         // +4 (14546): count down each day; [kGuardTimer..]: the guards
};
IMAGE_FIELDS(Days, s.arrival_countdown, s.arrival_index, s.timers);
static_assert(mem::image::size<Days>() == 32);
inline constexpr int kGuardTimer = 9;  // timers 9-13 (a4+14564): reset to 3 each day while place 63 is on land

// Missions fought and their ends (the record's 6 bytes at +338; formerly
// a4+14574).
struct Battles {
  int16_t fought;  // +0 (a4+14574)
  int16_t won;     // +2 (14576)
  int16_t lost;    // +4 (14578)
};
IMAGE_FIELDS(Battles, s.fought, s.won, s.lost);

// How the campaign ended (6 bytes, copied from a table by the ending).
struct Outcome {
  uint8_t record_ending_rank;  // +0 (a4+14610): goes to the record's ending_rank
  uint8_t record_final_rank;   // +1: to its final_rank
  uint8_t final_rank;  // +2: the rank named on the career page
  uint8_t first_page;  // +3: the first message page
  int8_t picture;      // +4: the career picture
  int8_t badge;        // +5: -1 none
};
IMAGE_FIELDS(Outcome, s.record_ending_rank, s.record_final_rank, s.final_rank, s.first_page, s.picture, s.badge);

// The score, the reserves and the war's counts (the record's 36 bytes at
// +344; formerly a4+14580).
struct Scores {
  int16_t total;        // +0 (a4+14580): tally.lost_b summed
  int16_t total_b;      // +2 (14582): tally.lost_a summed
  int16_t reserve_a;    // +4 (14584)
  int16_t reserve_b;    // +6 (14586)
  int16_t day_count;    // +8 (14588): days of war
  int16_t missions;     // +10 (14590)
  int16_t divisor;      // +12 (14592): total / divisor is the score's base
  int16_t won;          // +14 (14594)
  int16_t lost;         // +16 (14596)
  uint16_t class_high;  // +18 (14598): count_class: 13-17
  uint16_t class_top;   // +20: 18 and more
  uint16_t class_low;   // +22: below 9
  uint16_t class_mid;   // +24: 9-12
  uint16_t factor;      // +26 (14606): the score
  uint16_t medals;      // +28 (14608): the medals held (medal_masks)
  Outcome outcome;      // +30 (14610)
};
IMAGE_FIELDS(Scores, s.total, s.total_b, s.reserve_a, s.reserve_b, s.day_count, s.missions, s.divisor, s.won, s.lost,
             s.class_high, s.class_top, s.class_low, s.class_mid, s.factor, s.medals, s.outcome);
static_assert(mem::image::size<Scores>() == 36);

// A position on the strategic map (map pixels; MapPoint in the emulated
// memory).
struct Position {
  int16_t x, y;
};
IMAGE_FIELDS(Position, s.x, s.y);

// The mission under way, the menu's request, the fronts kept between
// screens and where the submarine and units 0 and 1 were (the record's
// 44 bytes at +420; formerly a4+14656).
struct MissionState {
  int16_t order;           // +0 (a4+14656): a single mission's order; -1 the war phase's mission
  int16_t request;         // +2 (14658): what the menu asks for (kRequest*; input sets kRequestKey, kRequestQuit)
  int16_t request_copy;    // +4 (14660)
  int16_t situation_copy;  // +6 (14662)
  int8_t front_stage[8];   // +8 (14664): the fronts' state (save_fronts)
  int8_t front_last[8];    // +16 (14672)
  int8_t front_next[8];    // +24 (14680)
  Position positions[3];   // +32 (14688): the submarine, units 0 and 1
};
IMAGE_FIELDS(MissionState, s.order, s.request, s.request_copy, s.situation_copy, s.front_stage, s.front_last,
             s.front_next, s.positions);
static_assert(mem::image::size<MissionState>() == 44);

// The player's career while Red runs (from the player's record;
// formerly a4+16044..16061).
struct Player {
  int16_t rank;           // a4+16044: 0-4
  int16_t rank_bit;       // a4+16046: 0-7, the rank's step
  uint16_t rank_bits[5];  // a4+16048: by rank, the steps reached (bit_of)
  int16_t level;          // a4+16058: the game's level (speed_level)
  int16_t spread;         // a4+16060: how the enemy wanders (0-3; 2 and more: no going home)
};

// After a battle: the enemy group's value before and after it (by the
// two value tables, member_value_a and _b) and what it lost; the fronts'
// moves; the score (formerly a4+15646..15671).
struct Tally {
  int16_t lost_b;         // a4+15646: before_b - after_b
  int16_t before_b;       // a4+15648
  int16_t after_b;        // a4+15650
  int16_t lost_a;         // a4+15652: before_a - after_a
  int16_t before_a;       // a4+15654
  int16_t after_a;        // a4+15656
  int16_t fronts_moving;  // a4+15658: > 0: the fronts move
  int16_t fronts_other;   // a4+15660
  int16_t group_size;     // a4+15662: the enemy group's size in the last battle
  int16_t sunk;           // a4+15664
  int16_t hit;            // a4+15666
  int16_t score;          // a4+15668: sunk + hit
  uint16_t multiplier;    // a4+15670: the score's, by spread, level, order and rank
};

// The turn's flags (formerly words at a4+15500..15547).
struct Turn {
  uint16_t phase_none;      // a4+15500: the last phase was -1
  uint16_t war_phase9;      // a4+15502: the war's phase 9 or 10 (no reinforcements)
  uint16_t names_shown;     // a4+15504: the mission's names were built
  uint16_t no_pursuit;      // a4+15506
  uint16_t no_picture_b;    // a4+15510
  uint16_t turn_over;       // a4+15514
  uint16_t no_picture_a;    // a4+15516
  uint16_t war_flag;        // a4+15518: loadout.unit_bits & 0xf0 at the start
  uint16_t no_arrivals;     // a4+15520
  uint16_t unit2_warned;    // a4+15522: the effect for unit 2 was played
  uint16_t unit5_warned;    // a4+15524
  uint16_t fronts_settling; // a4+15528: fronts are being drawn to their end (bitmap 2 only)
  uint16_t unread_flag_a;   // a4+15530: cleared at the start (nothing reads it)
  uint16_t unread_flag_b;   // a4+15540: the same
  int16_t hours_to_run;     // a4+15544: run_minutes: hours still to play
  int16_t minutes_to_run;   // a4+15546
};

// A unit on the strategic map. Units 0 and 1 are the player's forces as
// seen by the enemy; unit 4 is the player's submarine (formerly 36-byte
// records at a4+14740).
struct MapUnit {
  uint16_t unknown_0;
  uint16_t shape;       // +2: low byte: its character in the map symbol font
  uint8_t status;       // +4: bit 7 arrived/in the view, bit 5 moved or in the view, bit 4 detected (seen)
  uint8_t unknown_5[3];
  int16_t mode;         // +8: what it is doing / its animation frame (2, 18, 34, 49 when hit)
  uint16_t unknown_10;
  int16_t anim_timer;   // +12
  uint16_t range;       // +14: detection range
  uint16_t bit;         // +16: 1 << its number
  uint8_t shown[2];     // +18: drawn in bitmap 0 / 1
  Position offset;      // +20: subtracted from pos where it is drawn
  Position pos;         // +24: position on the map
  Position drawn[2];    // +28: where it was last drawn in bitmaps 0 and 1
};

// A unit's movement on the strategic map (formerly 44-byte records at
// a4+15104).
struct UnitRecord {
  int16_t state;         // +0: -16 idle, -1 arrived, else moving; negative: not in play
  uint8_t flags;         // +2: bit 7 order advanced, bit 1 moving up (y < 0), bit 0 moving left (x < 0)
  uint8_t heading;       // +3: bits 4-7 up, down, left, right (the main axis: see passed)
  uint8_t unknown_4[2];
  int16_t time;          // +6: frames until the next order (32767: none)
  int16_t from;          // +8: the place it comes from (-1: none)
  int16_t dest;          // +10: the place it is moving to, the next on its route (-1: none yet)
  int16_t target;        // +12: where its route ends (the heading bits point there from dest)
  int16_t speed;         // +14 (unit 4: -1 the cursor stays on the submarine)
  int16_t hunts;         // +16: nonzero: it hunts the submarine (kHunter)
  int16_t route_pos;     // +18: unit 6: its position in its route list
  int16_t max_speed;     // +20
  uint16_t step;         // +22: added to `progress` every tick (speed * 8)
  uint16_t progress;     // +24: movement accumulator; a carry out of the low byte is one map step
  int16_t speed_class;   // +26: 2 without a target, 3 with one
  int16_t patrol_y;      // +28: a patrolling unit's y (0-300, bouncing)
  int16_t patrol_x;      // +30: its x (cycling 0-1919)
  int16_t vy, vx;        // +32: per step, 256 = one pixel along the main axis
  uint16_t frac_y, frac_x;  // +36: fractions of a pixel (low byte kept)
  mem::ConstPtr<uint8_t> script;  // +40: its orders: place, target, speed, time, then the next ones (in Red's data)
};

// map_distance's results: the signs and sizes of the last difference
// (formerly a4+15618).
struct Distance {
  int16_t result;
  int16_t dx, dy;  // |dx|, |dy|
  uint32_t bits;   // bit 31 |dx| >= |dy|, 30 the opposite, 29 dx < 0, 28 dy < 0
  uint32_t signs;  // bit 31 dx < 0, bit 30 dy < 0
};

// Red's initialised data that the game changes, kept while the module
// stays loaded (taken from the loaded image at its first start after a
// load; RedStorm keeps the modules loaded when memory allows).

// A place on the strategic map (formerly 10-byte records at a4+1188).
struct Place {
  int8_t kind;               // +0: 0 sea, 1 near land (not for units with kAtSea), 2+ not on routes
  uint8_t unknown_1;
  Position pos;              // +2
  mem::ConstPtr<int8_t> route;    // +6: the places it leads to (ended by -1; in Red's data)
};
IMAGE_FIELDS(Place, s.kind, s.unknown_1, s.pos, s.route);
static_assert(mem::image::size<Place>() == 10);
inline constexpr int kPlaces = 69, kPlaceBytes = 10, kPlacePosOffset = 2;

// The front lines on the map (8). Each is a list of line segments; up to
// 4 walkers redraw it segment by segment in the advancing side's colour
// (formerly 90-byte records at a4-44180).
struct FrontWalker {
  int16_t segment;   // +0: the segment it walks (-1: done)
  int16_t steps;     // +2: left on the segment
  int16_t counter;   // +4: steps until the minor axis moves
  int16_t period;    // +6: the minor axis moves every `period` steps
  Position pos;      // +8
  uint32_t dir;      // +12: map_distance's bits (kXMajor, kNegX, kNegY)
};
IMAGE_FIELDS(FrontWalker, s.segment, s.steps, s.counter, s.period, s.pos, s.dir);
struct MapPoint;
struct Front {
  mem::ConstPtr<MapPoint> points;  // +0: segment k runs from points[2k] to points[2k + 1] (in Red's data)
  uint8_t unknown_4[2];
  int16_t last;            // +6: the segment the walk stops at
  int16_t next;            // +8: the next segment to walk
  int16_t current;         // +10
  int16_t countdown;       // +12: frames to the next step
  int16_t walking;         // +14: walkers still walking
  int16_t stage;           // +16: 0-6, how far the front has moved
  FrontWalker walkers[4];  // +18
  int16_t colour_from;     // +82: redrawn from this colour
  int16_t colour_to;       // +84: to this one
  uint8_t flags;           // +86: kFrontActive, kFrontAscending
  uint8_t unknown_87[3];
};
IMAGE_FIELDS(Front, s.points, s.unknown_4, s.last, s.next, s.current, s.countdown, s.walking, s.stage, s.walkers,
             s.colour_from, s.colour_to, s.flags, s.unknown_87);
static_assert(mem::image::size<Front>() == 90);

struct Data {
  Front fronts[8];           // (a4-44180)
  Place places[kPlaces];     // (a4+1188)
  uint8_t text_page[2000];   // (a4-43160): 25 rows of 40 characters, then their 40 colours
  uint16_t mission_eligible[70];  // (MissionRule::eligible, a4-49108 + 12 m): set by pick_mission
  int8_t kind_before;        // the byte before the places (a4+1178): next_target reads it as place -1's kind
  int16_t unread_word;       // (a4+134): cleared at the module's start (nothing reads it)
};
inline Data data;
inline Front (&fronts)[8] = data.fronts;
inline Place (&places)[kPlaces] = data.places;
inline uint8_t (&text_page)[2000] = data.text_page;
inline uint16_t (&mission_eligible)[70] = data.mission_eligible;
// A place's kind; place -1 (a route's end) gives the byte before the table.
inline int8_t place_kind(int16_t place) { return place < 0 ? data.kind_before : data.places[place].kind; }

// Where the image holds them.
inline constexpr int32_t kFrontsImage = -44180, kPlacesImage = 1188, kKindBeforeImage = 1178, kTextPageImage = -43160,
                         kMissionRulesImage = -49108, kMissionRuleBytes = 12;
inline constexpr mem::state::Range kMovedData[] = {
    {-44180, 720},   // fronts
    {1188, 690},     // places
    {-43160, 2000},  // text_page
    {134, 2},        // unread_word
};

struct State {
  Date clock;       // the campaign clock (was a4+14384)
  Phase phase;      // (a4+14394)
  Group groups[2];  // the battle groups (a4+14444; the record's +208)
  War war;          // (a4+14616)
  Loadout loadout;  // (a4+14492)
  Days days;        // (a4+14542)
  Battles battles;  // (a4+14574)
  Scores scores;    // (a4+14580)
  MissionState mission;  // (a4+14656)
  uint8_t units_left[32];  // (a4+16062; the record's +464): by unit, how many are left to form groups from
  Player player;           // (a4+16044)
  int8_t units_available[32];  // (a4+16094): units_left while a group is formed
  Tally tally;      // (a4+15646)
  Turn turn;        // (a4+15500)
  MapUnit map_units[10];       // (a4+14740)
  UnitRecord unit_records[9];  // (a4+15104)
  Distance distance;           // (a4+15618)
};

inline State state;
inline Date& clock = state.clock;
inline Phase& phase = state.phase;
inline Group (&groups)[2] = state.groups;
inline War& war = state.war;
inline Loadout& loadout = state.loadout;
inline Days& days = state.days;
inline Battles& battles = state.battles;
inline Scores& scores = state.scores;
inline MissionState& mission = state.mission;
inline uint8_t (&units_left)[32] = state.units_left;
inline Player& player = state.player;
inline int8_t (&units_available)[32] = state.units_available;
inline Tally& tally = state.tally;
inline Turn& turn_flags = state.turn;
inline MapUnit (&map_units)[10] = state.map_units;
inline UnitRecord (&unit_records)[9] = state.unit_records;
inline Distance& distance = state.distance;

// The a4 ranges of Red's small-data area that State replaces.
inline constexpr mem::state::Range kMovedRanges[] = {
    {14384, 10},  // clock
    {14394, 50},  // phase
    {14444, 48},  // groups
    {14492, 50},  // loadout
    {14542, 32},  // days
    {14574, 6},   // battles
    {14580, 36},  // scores
    {14656, 44},  // mission
    {16044, 18},  // player
    {15646, 26},  // tally
    {15500, 8},   // turn: 15500-15507
    {15510, 2},   // 15510
    {15514, 12},  // 15514-15525
    {15528, 4},   // 15528-15531
    {15540, 2},   // 15540
    {15544, 4},   // 15544-15547
    {14740, 360}, // map_units
    {15104, 396}, // unit_records
    {15618, 14},  // distance
    {16062, 32},  // units_left
    {16094, 32},  // units_available
    {14616, 40},  // war
};

// The fields scenario scripts set.
inline constexpr mem::state::Field kScriptFields[] = {
    {14404, 2, &state.phase.mission_needs},
    {14534, 1, &state.loadout.flags, 4},  // its top byte
    {14908, 2, &state.map_units[4].pos.x},  // the submarine
    {14910, 2, &state.map_units[4].pos.y},
};

}  // namespace game::red
