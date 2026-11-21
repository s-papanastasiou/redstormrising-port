// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red (the campaign module): records and variables used by the game code.
// Offsets are a4-relative in Red's small-data area.
#pragma once

#include "game/common/record.hpp"
#include "game/common/state.hpp"

#include "game/ui/state.hpp"

#include "game/mem/constants.hpp"
#include "game/mem/globals.hpp"
#include "game/red/state.hpp"
#include "game/screen/vars.hpp"
#include "game/screen/styles.hpp"

namespace game::red {

using namespace game::mem;

// A position on the strategic map, in map pixels (320 x 200).
struct MapPoint {
  sbe16 x;
  sbe16 y;
  // To and from the native Position (state.hpp).
  operator Position() const { return {x, y}; }
  MapPoint& operator=(const Position& p) {
    x = p.x, y = p.y;
    return *this;
  }
};
static_assert(sizeof(MapPoint) == 4);
inline MapPoint map_point(const Position& p) {
  MapPoint m;
  m = p;
  return m;
}
inline Position position(const MapPoint& m) { return m; }

// A place's position. pick_mission passes its range's place byte with
// the flag in bit 7 still set, so the original reads "places" past the
// table there: Red's other data, which stays in emulated memory.
inline Position place_pos(int16_t place) {
  if (place >= 0 && place < kPlaces) return places[place].pos;
  // (outside the table: the bytes the original read there, as loaded)
  return constant<MapPoint>(a4() + uint32_t(kPlacesImage + place * int32_t(kPlaceBytes) + kPlacePosOffset));
}

inline constexpr uint8_t kFrontActive = 0x80, kFrontAscending = 0x40;
inline constexpr int kFronts = 8, kWalkers = 4, kLastStage = 6;
inline constexpr int16_t kFrontColourA = 2, kFrontColourB = 9;

// UnitRecord flags and heading bits.
inline constexpr uint8_t kSeen = 0x10, kMoved = 0x20, kFrozen = 0x40, kArrived = 0x80;  // MapUnit status
inline constexpr uint8_t kAtSea = 0x40, kHunter = 0x10;  // UnitRecord flags: always detected (units 0, 1)
inline constexpr uint8_t kEvent = 0x04, kAlerted = 0x08;  // UnitRecord flags: an event is due, it was alerted
inline constexpr int16_t kIdle = -16, kArrivedState = -1;                // UnitRecord state
inline constexpr int16_t kPointer = 9;  // the unit that follows the mouse pointer

inline constexpr uint8_t kOrderAdvanced = 0x80, kMovingUp = 0x02, kMovingLeft = 0x01;
inline constexpr uint8_t kHeadingRight = 0x80, kHeadingLeft = 0x40, kHeadingDown = 0x20, kHeadingUp = 0x10;

inline constexpr int16_t kPlayerSub = 4;  // the player's submarine among the map units

inline constexpr Const<sbe16> front_stage_ends{-44292};    // [8][7]: a front's `last` segment by stage
inline constexpr uint32_t kSignX = 0x80000000u, kSignY = 0x40000000u;
inline constexpr uint32_t kXMajor = 0x80000000u, kYMajor = 0x40000000u, kNegX = 0x20000000u, kNegY = 0x10000000u;

inline constexpr int16_t kPlayerPlace = 67;          // a place that follows the player's submarine
inline constexpr int16_t kUnit8 = 8;

// The per-frame unit steps.
// The mission's type (phase.mission_kind's high nibble) as battle_won judges
// it; the target type's table (by the low nibble, Red's code hunk).
inline constexpr int16_t kMissionLosses = 2, kMissionLosses2 = 3, kMissionSinkType = 4, kMissionUnits = 5;
inline constexpr ConstData<sbe16> mission_target_types{{0, 0, 0, 0, 0x2fd44}};
inline constexpr int16_t kMemberSunk = 2;  // Group::results: a member sunk
inline constexpr Const<sbe16> sub_targets{-46106};     // [6] places, by the low nibble (Red's data)
inline constexpr Const<sbe16> sub_target_limits{-46100};  // [6] distances

// The part of the map in view (the menu cursor's limits are the same words).
struct View {
  sbe16 x, y, w, h;
};

// Routes.
inline constexpr Const<int8_t> unit3_route_52{5638};   // places around 52, 39, 35 (ended by -1)
inline constexpr Const<int8_t> unit3_route_39{5644};
inline constexpr Const<int8_t> unit3_route_35{5649};
// Unit 3 may head for 35 while front 3 has a `last` segment, for 39 while
// front 2 has one (the original reads these words of the fronts table).
inline constexpr int kFront35 = 3, kFront39 = 2;
inline constexpr Const<int8_t> unit6_routes{-43264};   // [16 per route] places (ended by -1)
inline constexpr Const<int8_t> unit6_starts{5656};     // places to start from (ended by -1)
inline constexpr int16_t kPatrolLength = 1920;  // patrol_x cycles through 0..1919
inline constexpr int16_t kLand = 2;                  // the map colour of land
inline constexpr int16_t kLost = -3;                 // UnitRecord state: no way on
inline constexpr uint16_t kRouteChosen = 0x8000;     // unit 6's state
inline constexpr int16_t kBase = 11;  // the place of the submarine's base
inline constexpr int16_t kEffectUnit3 = 14, kEffectUnit2 = 15, kEffectUnit5 = 16;  // Red's sound effects
inline constexpr int16_t kEffectNone = 0, kEffectBaseReport = 5, kEffectAlert = 18;

// The campaign's phases and battle groups.
inline constexpr Const<int8_t> phase_a_table{-46269};     // Red's data, by phase.war_phase
inline constexpr Const<int8_t> phase_b_table{-46199};
inline constexpr int16_t kPortPlace = 68;               // the sighting place: a dest of 68 does not count as a target
inline constexpr Const<int8_t> member_speeds{3212};       // [level][32]: by member
inline constexpr int16_t kFirstShipAtSea = 13;          // members from here on are at sea
inline constexpr Const<int8_t> mission_codes{-46588};     // Red's data: by mission (0-30)
inline constexpr Const<int8_t> mission_places{-46557};
inline constexpr Const<int8_t> mission_timer_index{547};

// The campaign clock.
inline constexpr Const<sbe16> month_lengths{5576};     // [12]
inline constexpr int16_t kGuardPlace = 63;
inline constexpr Const<sbe16> arrivals{5110};            // [5]: high nibble / low nibble taken from the counters
inline constexpr Const<sbe16> arrival_messages{5100};    // [5]
inline constexpr Const<sbe16> reinforce_thresholds{3192};  // [n]: reinforce_chance
inline constexpr int kArrivals = 5, kGuards = 5;

// The current menu of a text page (choose, highlight): two columns of
// entries; the keys word's low byte holds direction bits while choosing.
struct MenuEntry {
  uint8_t row, first, last, code;  // text page row, first and last column, what it chooses
};
inline constexpr Const<MenuEntry> menu_split{5684}; // a list whose columns share rows (20 is the split)
inline constexpr uint8_t kRightSide = 0x80, kEntryNormal = 1, kEntryLit = 3;

// Row menus of RedStart and Red (add_row): rows of a box.
struct RowMenu {
  sbe16 x, y;          // +0: the first row's position
  sbe16 x_end, y_end;  // +4: the grid's end
  sbe16 column;        // +8: between the two columns (0: one column)
  be16 step;           // +10: between rows
  sbe16 width, height; // +12
};
inline constexpr int16_t kSecondColumn = 10;  // row menu entries of the second column
// Packed texts (texts.cpp): a length byte that counts itself, then five
// bits a character.
inline constexpr ConstData<sbe16> message_offsets{{0, 0, 0, 0x31ff2, 0x34202}};  // [n]: from message_texts
inline constexpr ConstData<uint8_t> message_texts{{0, 0, 0, 0x2cf80, 0x343c4}};
inline constexpr ConstPerModule<char> no_message{{0, 0, 0, -15446, 11994}};   // message 0
inline constexpr ConstData<sbe16> message2_offsets{{0, 0, 0, 0, 0x3c91a}};  // Red's second list
inline constexpr ConstData<uint8_t> message2_texts{{0, 0, 0, 0, 0x313b4}};
inline constexpr Const<char> no_message2{11992};
// A block of a menu page (6 words; a count of 0 ends the list).
struct PageBlock {
  sbe16 count;   // lines (texts)
  sbe16 first;   // the first text's number
  sbe16 unused;
  sbe16 x, y;
  sbe16 step;    // between lines; 0: a selectable row
};
inline constexpr int16_t kFooterA = 31, kFooterB = 32;
inline constexpr uint8_t kRemove = 0x80, kEscape = 255;
inline constexpr int16_t kTubeFixed = -2, kTubeFree = -1, kNotLoadable = 5;

// Text output on the text page (through the current style).
inline constexpr Const<char> name_list{13523};         // names, NUL-separated (print_name)
inline constexpr Const<char> one_char_template{-12};   // Red's data: a one-character string
inline constexpr uint8_t kBlankCell = 224, kNameEnd = '*';
inline constexpr int kPageRows = 25, kPageColumns = 40;
inline constexpr int16_t kPageWidth = 319;

inline constexpr Const<char> status_prefix{12864};     // print_status_word: "Navigation shows "
inline constexpr Const<char> status_words{13723};      // its words ("open water.", ...), NUL-separated

// The loadout screens: the submarine's weapons, its torpedo tubes (Loadout).
// loadout.flags:
inline constexpr uint32_t kTubes5to8 = 1u << 31, kShortList = 1u << 30, kRow2_29 = 1u << 29, kRow28 = 1u << 28,
                          kRow3 = 1u << 27, kRow5 = 1u << 26, kRow1 = 1u << 25;
inline constexpr Const<sbe16> load_minutes{5880};       // [k]: minutes to load a weapon of kind k
inline constexpr Const<sbe16> weapon_order{5866};       // kinds in the order listed (ended by -1)
inline constexpr Const<sbe16> tube_masks{5902}, tube_hours{5894};  // [i]
inline constexpr Const<char> text_no{12104}, text_gap{12108}, text_colon{12112}, text_add{12116},
    text_remove{12120}, text_min{12128}, plural_e{12134}, plural_s{12136}, text_er{12182},
    text_war_day{12188}, text_pact{12198}, text_nato{12204};
inline constexpr Const<char> tube_choices{12455};       // Red's data: "All", "(1)", "(5)" (4 bytes each)
inline constexpr Const<char> text_operational{12727}, text_inoperative{12713}, text_hours{12741},
    text_tubes_at{12748}, text_torpedo_tubes{12820}, text_summary_a{13321}, text_summary_b{13435},
    text_sail_magazine{13483}, text_total_on_board{13501}, text_header{13891}, text_gauge_block{13945};
inline constexpr Const<char> plural_template{-10};      // Red's data: a one-character string
inline constexpr int16_t kTubeEmpty = -2;

// The text page: 25 rows of 40 characters, then their 40 colours.
inline constexpr int kPageRow = 80, kPageColours = 40;
// A byte of the text page by index. The original does not check the
// cursor: an index outside the page (a column past 2000 bytes) writes
// Red's data after it, which stays in emulated memory.
inline uint8_t& page_byte(int32_t i) {
  return i >= 0 && i < int32_t(sizeof(text_page)) ? text_page[i] : view<uint8_t>(a4() + uint32_t(kTextPageImage + i));
}

// Missions (6 bytes each).
struct Mission {
  int8_t weight;      // +0: its chance in pick_mission
  uint8_t unknown_1;
  ConstPtr<int8_t> chars;  // +2: characters to pick from (pick_mission_char)
};
static_assert(sizeof(Mission) == 6);
inline constexpr Const<Mission> missions{-47918};

// Battle groups 0 and 1: members and records.
// a4+15554 (64 bytes) is never written: the original copies zeros from
// it to clear records (struct assignment from a zero block). C++ clears them.
inline constexpr Const<be32> type_masks{3468};          // [type]: the units of a type, bit 31 = unit 0
inline constexpr uint32_t kUnit0Bit = 0x80000000u;
inline constexpr Const<int8_t> group_scripts_0{-47148}; // Red's data: 8 bytes per mission
inline constexpr Const<int8_t> group_scripts_1{-45604};
inline constexpr Const<int8_t> group_hunts{-46094};     // [2][70] by mission
inline constexpr int kGroupMembers = 5, kScriptBytes = 8, kHuntRow = 70;
inline constexpr int16_t kGroupShapeAtSea = 34, kGroupShape = 35;

// Drawing the units on the map: a text style for their symbols.
inline constexpr screen::StyleVar symbol_style{52};  // its +2.. (x, y, width, height) are the map view
inline constexpr int16_t kMapUnits = 9, kNotDrawn = 8;

// The turn and the map screen.
inline constexpr Const<char> situation_text_none{12212};  // the map screen's heading for situation -1
inline constexpr Const<char> situation_text_40{12425};
inline constexpr Const<char> situation_text{12295};
inline constexpr Const<be16> map_palette{10244};          // [32]
inline constexpr Const<uint8_t> name_table_a{3980}, name_table_b{4095}, name_table_c{4209};
inline constexpr int16_t kRequestNear = 5, kRequestMeet = 6, kEffectMeet = 26;

// Text pages (RedStart, Red: storm, redstorm, title, redstart, red).
inline constexpr ConstPerModule<char> picture_prefix{{0, 0, 0, -15310, 6046}};  // "NL": texts with their own picture
inline constexpr int16_t kPagePicture = 3;
inline constexpr Const<char> text_uss{13934};            // "USS " (message 10)
inline constexpr int16_t kMessageWithName = 10;
inline constexpr int16_t kMapPicture = 2;

// The campaign page and the daily messages.
inline constexpr Const<uint8_t> day_messages_a{4981}, day_messages_b{4911};  // [day]: by the war's situation
inline constexpr Const<uint8_t> briefings_first{4533}, briefings_second{4463}, briefings_dest_a{4323},
    briefings_dest_b{4393};                               // [day]
inline constexpr int16_t kBriefingDone = 25, kBriefingMany = 11, kBriefingFew = 12;
inline constexpr Const<char> text_top_secret{12140}, text_colon2{12152}, text_cmdr{12156}, text_usn{12162},
    text_uss2{12168};
inline constexpr Const<char> page_heading{12499}, text_press_trigger{12467};  // Red's data
inline constexpr int16_t kBannerX = 237, kBannerRight = 319, kBannerBottom = 9, kBannerTextX = 239, kBannerTextY = 2;

// The control room page (the briefing).
inline constexpr Const<char> text_conroom{12174};       // "CONROOM": its text page
inline constexpr Const<char> conroom_start{13164}, conroom_no_contact{13204}, conroom_battle_stations{13030},
    conroom_launch_window{13065}, conroom_continue{13002}, conroom_menu{13257}, conroom_contact{12888},
    bearing_digits{12953};                            // Red's data
inline constexpr int16_t kRequestContact = 0, kRequestLaunch = 5, kBearingAt = 52;

// The ship status (debriefing) page and the news.
inline constexpr Const<char> ship_status_text{12569}, ship_status_short{12835};  // Red's data
inline constexpr Const<uint8_t> news_day_a{4673}, news_day_b{4603};       // [day] messages
inline constexpr Const<uint8_t> news_ahead{4806}, news_ahead_a{4827}, news_ahead_b{4869};  // by reserve a
inline constexpr Const<uint8_t> news_behind{4743}, news_behind_a{4764}, news_behind_b{4785}; // by reserve b

// Medals, promotion and the campaign's end.
inline constexpr Const<be16> medal_scores{5230};
inline constexpr ConstPerModule<be16> medal_masks{{0, 0, 0, 12, 5242}};   // [6]
inline constexpr Const<sbe16> medal_names{5254}, medal_heights{5266}, medal_widths{5278};  // [6]
inline constexpr ConstPerModule<sbe16> medal_shapes{{0, 0, 0, 0, 5300}};  // [6] (RedStart: at a4+0)
inline constexpr PerModuleMixed<uint16_t, be16> medals_held{{0, 0, 0, 28, 0},
                                                            {nullptr, nullptr, nullptr, &ui::states[3].medals_held, &state.scores.medals}};
inline constexpr Const<be16> medal_palette{10308};      // [32]
inline constexpr int16_t kMedalHeading = 161, kMedalFrameShape = 6, kMedalFrameX = 128, kMedalFrameY = 80,
                         kMedalAreaW = 66, kMedalAreaH = 77, kMedals = 6;
inline constexpr Const<uint8_t> rank_full{5663};        // [rank]: the bits after a full set
inline constexpr int16_t kRankMessages = 165;
inline constexpr int kNameBytes = 16;
inline constexpr int16_t kEndWon = 127, kEndDraw = 126, kEndLost = 125;

// The player's record (passed between the modules: the career, and the
// campaign's state when one is under way).
struct PlayerRecord {
  be16 started;            // +0: 1 a game is under way
  sbe16 order;             // +2: a single mission's order (random for 10)
  sbe16 spread;            // +4: player.spread
  be16 rank;               // +6: rank * 8 + rank bit
  sbe16 level;             // +8: player.level
  sbe16 month;             // +10: the clock's month - 1
  sbe16 day;               // +12
  char commander[32];      // +14
  char sub_name[16];       // +46
  sbe16 terrain;           // +62: 1 coast, 2 land near, 3 open sea, 0 the fronts (count_terrain)
  sbe16 enemy_heading;     // +64
  sbe16 group_speed;       // +66
  sbe16 unit8_heading;     // +68
  sbe16 count_70;          // +70
  sbe16 words_72[5];       // +72
  sbe16 words_82[5];       // +82
  sbe16 player_speed;      // +92
  sbe16 player_heading;    // +94
  be16 unit_bits;          // +96
  be16 tubes[8];           // +98: low 7 bits: the weapon + 1
  sbe16 weapons[7];        // +114
  be16 score;              // +128: scores.factor at the campaign's end
  be16 medals;             // +130
  be16 rank_bits[5];       // +132
  be16 final_rank;         // +142: the rank in the hall of fame
  be16 ending_rank;        // +144: the rank in the saves and the hall of fame (0xff: none yet)
  sbe16 situation;         // +146: -1 no campaign under way
  uint8_t clock[10];       // +148
  uint8_t phase[50];       // +158
  uint8_t groups[48];      // +208
  uint8_t weapons_etc[50]; // +256
  uint8_t days[32];        // +306
  uint8_t battles[6];    // +338
  uint8_t scores[36];      // +344
  uint8_t war[40];         // +380
  uint8_t mission[44];     // +420
  uint8_t units_left[32];   // +464
};
static_assert(sizeof(PlayerRecord) == 496);
inline constexpr Const<int8_t> starting_weapons{-43460};  // Red's data: [level][rank][7]
inline constexpr Const<sbe16> starting_total{5856};       // [rank]
inline constexpr Const<sbe16> mul_by_spread{5052}, mul_by_level{5060}, mul_by_order{5068}, mul_by_rank{5090};

// The career page.
inline constexpr Const<char> rank_names{13808};          // Red's data: NUL-separated
inline constexpr Const<char> text_final_rank{13861};     // "Your Final Rank:"
// The endings (pages.cpp): tables of Outcome images.
inline constexpr int kOutcomeBytes = 6;
inline constexpr Const<uint8_t> war_outcomes{5176};     // [9][kOutcomeBytes]: the war's end
inline constexpr Const<uint8_t> single_outcomes{5120};  // [8][kOutcomeBytes]: a single mission's end
inline constexpr Const<uint8_t> single_medals{5168};    // [8]
inline constexpr int16_t kMessageOutcome = 204, kMessageMissionOver = 218, kOutcomeSeconds = 60;
inline constexpr int16_t kLateOrders = 8, kLastWarPhase = 9, kLongWar = 13, kSpreadHigh = 3;
inline constexpr Const<sbe16> career_pictures{5290}, badge_shapes{5312};
inline constexpr int16_t kCareerFrameShape = 57, kCareerFrameX = 91, kBadgeX = 280, kBadgeY = 39;
inline constexpr int16_t kMessageWon = 158, kMessageLost = 159, kMessageCareer = 164, kMessageCareerLate = 216,
                         kMessageTally = 162;

// Picking the next mission (70 of them).
struct MissionRule {
  be16 eligible_image;  // its initial value (pick_mission sets mission_eligible)
  be16 when_a, when_b;  // allowed in the first / second half (a coin toss)
  be16 by_reserve_a, by_reserve_b, by_place;  // which of the ranges below apply
};
static_assert(sizeof(MissionRule) == 12);
struct MissionRange {
  int8_t a_min, a_max, b_min, b_max;  // reserves
  uint8_t place;      // bit 7: the place must be on land, else at sea
};
static_assert(sizeof(MissionRange) == 5);
inline constexpr Const<MissionRule> mission_rules{-49108};   // Red's data
inline constexpr Const<MissionRange> mission_ranges{-48268};
inline constexpr int16_t kMissions = 70, kMissionGoHome = 31, kMissionLow = 9, kMissionHigh = 10;

// Ship pictures (the OSG file).
// The ship pictures (RedStart, Red).
inline constexpr ConstPerModule<char> osg_file{{0, 0, 0, -14880, 11932}}, osg_overrun_text{{0, 0, 0, -14912, 11900}},
    osg_missing_text{{0, 0, 0, -14866, 11946}};
inline constexpr int16_t kNoShip = 28, kShipX = 166;

// A single mission (situation 30) from an order.
inline constexpr Const<int8_t> order_groups{630};       // [order * 4 + spread][5]: the group's types
inline constexpr Const<int8_t> order_terrain_odds{550}; // [k][4]
inline constexpr Const<int8_t> terrain_by_odds{614};    // [i][4]: the terrain counts
inline constexpr int16_t kRandomOrder = 10, kSingleMission = 30;

// The port (resupply and repairs at the base).
inline constexpr Const<MenuEntry> port_menu_left{5744}, port_menu_right{5800};
inline constexpr int16_t kPortPlace10 = 10;            // the submarine is put on this place
inline constexpr uint8_t kCodeWeapons = 0x10, kCodeTorpedoes = 0x20, kCodeRepair = 0xf0, kCodeSpecial = 224;

// A new campaign and its missions.
inline constexpr Const<sbe16> start_years{5602};        // [level]: the clock's year (BCD)
inline constexpr Const<uint8_t> units_by_level{3340};   // [level][32]: units available
inline constexpr Const<int8_t> mission_groups_0{-47498}, mission_groups_1{-45954};  // Red's data: [mission][5] types
inline constexpr int16_t kStartMonth = 6, kStartDay = 1, kStartHour = 6;

// The map units' symbol offsets (from where their position is drawn).
inline constexpr Const<MapPoint> unit_offsets{5610};  // [7]
inline constexpr int16_t kUnitCount = 10;

// A battle's result.
inline constexpr Const<int8_t> won_gains{-46549}, lost_losses{-46409};  // Red's data: [mission][2] (a, b)

// Unit 7's route (positions; it moves on while the day timer of its
// stop is running) and the place that marks where the submarine was seen.
inline constexpr Const<MapPoint> unit7_route{-43320};    // Red's data: [14]
inline constexpr int kUnit7Stops = 14;
inline constexpr int16_t kSightingPlace = 68;

// The tally after a battle (Storm's results per member: 1 hit, 2 sunk).
inline constexpr Const<int8_t> member_value_a{5320}, member_value_b{5448};  // [level][32]
inline constexpr ConstPerModule<be32> picture_names{{0, 0, 0, -15358, 5998}};   // [12] "CAP", "CONROOM", "MAP", ...
inline constexpr int kPictures = 12;
inline constexpr Const<MenuEntry> conroom_menu_list{5668};
inline constexpr uint8_t kConroomLeave = 240, kConroomLoadout = 241, kConroomCampaign = 242, kConroomNoBattle = 243;

// recolour_rect (Red, RedStart): blitter masks and minterms and a
// temporary plane.
inline constexpr ConstPerModule<be16> recolour_first_masks{{0, 0, 0, -15556, -92}};  // [16], then the last-word masks [16]
inline constexpr ConstPerModule<uint8_t> recolour_minterms{{0, 0, 0, -15492, -28}};  // [8]
inline constexpr uint16_t kBlitABCD = 0x0f00, kBlitABD = 0x0d00, kBlitAC_D_Flip = 0x0b5a, kBlitDOnly = 0x0100;

inline constexpr Const<char> text_tally{12385};          // Red's data: before the tally's number

// Screen modes (the map with its animation, or a text page).
inline constexpr screen::StyleVar page_style{28};        // the text page's style


inline constexpr uint32_t kCursorMoved = 1u << 30;  // menu_flags
inline constexpr uint32_t kMenuFire = 1u << 29;     // menu_flags: fire held
inline constexpr uint32_t kKeyRead = 1u << 31;      // menu_flags: a key was read this frame

// Map pixel colours the submarine may enter (sea and ice shades).
inline constexpr int16_t kNavigable[] = {0, 1, 26, 27};
inline constexpr int16_t kMapBitmap = 2;  // select_entry index of the map's bitmap

// Red's (and RedStart's) names for the shared UI state they use
// (game::ui::State, the menu and frame-tick code's variables).
inline int16_t& map_active() { return ui::state().mouse_steering; }     // frame_tick animates the map
inline int16_t& map_controls() { return ui::state().space_requests; }   // the map's menu controls are on
inline int16_t& map_redrawn() { return ui::state().frame_ticked; }      // the map was redrawn this frame
inline int16_t& map_draw_bitmap() { return ui::state().flip_bitmap; }   // the map's bitmap drawn into (frame_tick shows it, then flips bit 0)
inline int16_t& map_ticks() { return ui::state().tick_frames; }         // counts map updates; the submarine moves every 8th
inline int16_t& map_count() { return ui::state().flips; }  // bit 3: units 0 and 1 blink
inline int16_t& map_flip_countdown() { return ui::state().flip_countdown; }
inline int16_t& map_double_buffer() { return ui::state().double_buffer; }  // frame_tick flips the bitmaps
inline int16_t& menu_dirs_red() { return ui::state().menu_dirs; }       // the menu's direction bits (input)
inline int16_t& no_help_request() { return ui::state().no_help_request; }
inline int16_t& quit_key_pressed() { return ui::state().quit_key; }        // Red's a4+0: Alt-Q was pressed
inline ui::Point& menu_mouse_pos() { return ui::state().mouse_pos; }
inline int16_t& mouse_clicked() { return ui::state().button_event; }  // the mouse moved or a button changed
inline uint32_t& temp_plane() { return ui::state().planes_buffer_copy; }  // a plane of scratch chip memory
inline ui::Point& map_cursor() { return ui::state().menu_cursor; }      // where the player steers (the menu cursor)
inline ui::Point ui_point(const Position& p) { return {p.x, p.y}; }
inline Position position(const ui::Point& p) { return {p.x, p.y}; }

// The player's record (common/record.hpp), as Red reads it.
inline PlayerRecord* career() { return common::player_record_as<PlayerRecord>(); }

}  // namespace game::red
