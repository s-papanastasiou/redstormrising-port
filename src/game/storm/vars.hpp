// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Storm (the battle module): variables used by the game code. Offsets are
// a4-relative in Storm's small-data area.
#pragma once

#include "game/common/record.hpp"
#include "game/common/state.hpp"

#include "game/mem/constants.hpp"
#include "game/mem/globals.hpp"
#include "game/screen/vars.hpp"
#include "game/screen/styles.hpp"
#include "game/storm/state.hpp"

namespace game::storm {

// Storm's sound effects (audio::play_effect; the samples are Storm's).
inline constexpr int16_t kEffectSilenceAll = 0, kEffectSilence = 1, kEffectSoundOn = 2;
inline constexpr int16_t kEffectExplosionDeep = 6, kEffectExplosion = 7;  // a submarine hit; a ship hit, ice
inline constexpr int16_t kEffectBlow = 8;            // a blow to the player's submarine
inline constexpr int16_t kEffectBeep = 13, kEffectRadar = 14;
inline constexpr int16_t kEffectTransient = 17;      // an enemy launch heard
inline constexpr int16_t kEffectEnemyTorpedo = 18;
inline constexpr int16_t kEffectPings[3] = {20, 21, 22};  // an enemy's active sonar, by its guidance
inline constexpr int16_t kEffectSonobuoy = 22;
inline constexpr int16_t kEffectCountermeasure = 23;  // a noisemaker or decoy launched
inline constexpr int16_t kEffectHullCreak = 24;       // below the depth limit
inline constexpr int16_t kEffectLaunch = 26;          // the player fires
inline constexpr int16_t kEffectOwnPing = 27;         // the player's active sonar
inline constexpr int16_t kEffectTorpedoApproaching = 28;

// The event pictures (show_event: the picture sequences of Storm).
inline constexpr int16_t kPictureMissileLaunch = 1, kPictureRbuAttack = 2, kPictureShipHitMissile = 3,
                         kPictureShotDown = 4, kPictureSubmarineHit = 5, kPictureTorpedoLaunch = 6,
                         kPictureShipHitTorpedo = 7, kPictureSubmarineSunk = 8, kPictureSunkByRbu = 9,
                         kPictureShipSunkMissile = 10, kPictureShipSunkTorpedo = 11;

using namespace game::mem;


inline constexpr int16_t kSilentSubMode = 1;


// A ship type (34 bytes).
struct ShipType {
  be16 name;         // +0: in type_names
  sbe16 kind;        // +2: kSubmarineKind, kSurfaceShipKind, kAircraftKind
  sbe16 noise;       // +4
  sbe16 rating[3];   // +6: per weapon kind (0 torpedoes, 1, 2); [0] is also its sensors
  sbe16 arc[3];      // +12: per weapon kind, the most it can fire off its heading (1/256 turns)
  sbe16 guidance[3]; // +18: per weapon kind, 0-2
  sbe16 defence;     // +24
  sbe16 air_defence; // +26: against missiles
  sbe16 max_speed;   // +28
  uint8_t unknown_30;
  uint8_t weapons;   // +31: kHasTorpedoes, kHasMissiles, kHasHelicopter, kHasRbu
  sbe16 toughness;   // +32: against hits; also the sonar shades' step (/ 16 + 2)
};
inline constexpr uint8_t kHasTorpedoes = 0x01, kHasMissiles = 0x02, kHasHelicopter = 0x04, kHasRbu = 0x08;  // ShipType::weapons
static_assert(sizeof(ShipType) == 34);
inline constexpr int32_t kShipTypesAt = -29712;
inline constexpr int16_t kTorpedoType = 2;  // the player's weapons' ShipType
// The ship types: a table of the constant data, but for the torpedo's
// entry, which the battle sets up (noise, defence, speed by the boat and
// setting): that one is state (initialised data, state.cpp).
ShipType& torpedo_type();
struct ShipTypes {
  const ShipType& operator[](int i) const {
    return i == kTorpedoType ? torpedo_type() : Const<ShipType>{kShipTypesAt}[i];
  }
};
inline constexpr ShipTypes ship_types{};
inline constexpr Const<be32> class_names{-32376};  // [ShipType.kind]
inline constexpr Const<be32> type_names{-31966};   // [ShipType.name]
inline constexpr Const<char> text_contact{-26128}, text_contact_lost{-26120};
inline constexpr int16_t kSubmarineKind = 1, kSurfaceShipKind = 2, kAircraftKind = 3;  // ShipType::kind
inline constexpr int16_t kPeriscopeDepth = 56;

// The conditions attack_score works with.
inline constexpr Const<be32> strength_names{-30986};  // [strength]
inline constexpr Const<sbe16> duct_colours{-31038};        // [duct_strength]
inline constexpr int16_t kSureScore = 99, kNoShot = -999;
inline constexpr char kBelowCode = 24, kAboveCode = 25;



inline constexpr uint16_t kLogOwn = 0x100, kLogShip = 0x200, kLogWeapon = 0x400, kLogObject = 0x800;  // what an entry is (| the id)
inline constexpr uint16_t kLogLaunched = 0x1000, kLogDamaged = 0x2000, kLogSunk = 0x4000;  // a ship entry's events
inline constexpr int16_t kObjects = 14, kLogFull = 2025;

// The battle's display modes and selection.
// The player's submarine's damage: bits 0-3 damaged systems (kSystem*),
// 5 an iceberg, 6 aground, 7 sunk.
// battle.damage_word: the player's systems out of order, and the end.
inline constexpr uint8_t kDamageTowedArray = 0x01, kDamagePassiveSonar = 0x02, kDamageTubes = 0x04,
                         kDamagePropulsion = 0x08;
inline constexpr uint8_t kAllSystems = 15;
inline constexpr uint8_t kDamageSunkShown = 0x10;  // sunk by a weapon: its final picture was shown
inline constexpr uint8_t kDamageLost = 0x80;       // the submarine is lost: the battle ends
inline constexpr Const<be32> mode_names{-32450};  // [mode]
inline constexpr Const<be32> start_messages{-30776};  // [i]: start_message's texts
inline constexpr Const<sbe16> entry_lengths{-30856};   // [entry mode]: its most digits
inline constexpr Const<sbe16> entry_x{-30848};         // [entry mode]: where its digits are shown
inline constexpr int16_t kEntryCourse = 1, kEntryDepth = 2, kEntryTube = 3;
inline constexpr Const<sbe16> ice_depths{-31080};
inline constexpr Const<be16> battle_palette{-11576};  // [32]
// The battle palette's pens (battle_palette, $0RGB), by colour.
inline constexpr int16_t kPenBlack = 0, kPenWhite = 1, kPenDarkRed = 2, kPenCrimson = 3;  // 000 fff 900 c02
inline constexpr int16_t kPenViolet = 4, kPenPurple = 5, kPenPlum = 6, kPenYellow = 7;  // 50b 70a 809 ff4
inline constexpr int16_t kPenLime = 8, kPenGreen = 9, kPenTeal = 10, kPenAqua = 11;  // bf0 0f0 097 0da
inline constexpr int16_t kPenCyan = 12, kPenSky = 13, kPenAzure = 14, kPenBlue = 15;  // 0ff 0bf 06f 00f
inline constexpr int16_t kPenIndigo = 16, kPenRed = 17, kPenOrange = 18, kPenGold = 19;  // 30d f22 f60 fd0
inline constexpr int16_t kPenMagenta = 20, kPenRaspberry = 21, kPenSea = 22, kPenPaleBlue = 23;  // 908 b06 08a adf
inline constexpr int16_t kPenLightBlue = 24, kPenOcean = 25, kPenNavy = 26, kPenDeepSea = 27;  // 5cf 09d 16b 048
inline constexpr int16_t kPenLightGrey = 28, kPenGrey = 29, kPenDarkGrey = 30, kPenCharcoal = 31;  // ccc 999 555 333

inline constexpr Const<char> text_rbu_attack{-26086}, text_no_torpedos{-25920};
inline constexpr Const<char> text_fire2{-24726}, text_bearing_word2{-24720}, text_range_word2{-24712};
inline constexpr int16_t kEmptyGauge = -116;
inline constexpr Const<be32> sensor_names{-31312};  // [sensor]
inline constexpr Const<char> text_contact_dim{-27368}, text_contact_lit{-27360};
inline constexpr Const<char> text_contact_label{-27452}, text_none3{-27444}, text_c_bearing{-27436},
    text_c_deg{-27428}, text_sig_snsr{-27420}, text_sol{-27410}, text_percent{-27402}, text_crs_spd{-27400},
    text_crs_spd_none{-27392}, text_c_range{-27384}, text_kyds_none{-27376};
inline constexpr Const<char> text_select_contact2{-25560}, text_sonar_analysis{-25542}, text_target_class{-25516},
    text_undetectable{-25500}, text_tracking_range{-25486}, text_detection{-25470}, text_threshold{-25460},
    text_g_active{-25450}, text_g_passive{-25442}, text_g_towed{-25434}, text_g_active2{-25428},
    text_g_passive2{-25420}, text_g_towed2{-25412};

// The after action report.
inline constexpr Const<char> text_to_comsublant{-24302}, text_re_report{-24284}, text_enemy_engaged{-24258},
    text_result{-24242}, text_departed{-24234}, text_escaped{-24224}, text_damaged{-24216},
    text_shot_down{-24208}, text_departed2{-24198}, text_sunk{-24188}, text_press_continue{-24182};

// The general quarters report before a battle.
struct BattleStart {
  uint8_t unknown_0[2];
  sbe16 level;              // +2: above 7, no damage
  sbe16 difficulty;         // +4: (& 3) the player's difficulty
  sbe16 class_6;            // +6: (>> 3) the submarine's class
  sbe16 era;                // +8: (& 3) the year
  uint8_t unknown_10[52];
  sbe16 sea_mode;           // +62: depth_mode (0 sea bed, 1 ice, 2 ...)
  sbe16 route_direction;    // +64: the enemy group's course (index into contact_bearings)
  uint8_t unknown_66[2];
  sbe16 contact_direction;  // +68: index into contact_bearings
  sbe16 enemy_ships;        // +70
  sbe16 enemy_types[5];     // +72: indices into the setting's ship list
  uint8_t unknown_82[10];
  sbe16 own_speed;          // +92
  sbe16 own_direction;      // +94: index into contact_bearings
};
inline constexpr Const<sbe16> contact_bearings{-31070};        // [direction]
inline constexpr Const<sbe16> tube_text_x{-24758}, tube_text_y{-24742};  // [line]
inline constexpr Const<be32> weapon_short_names{-24912};  // [weapon]
inline constexpr Const<be32> sea_mode_names{-24838};      // [depth_mode]
inline constexpr Const<be32> strength_words{-24782};      // [strength]
inline constexpr Const<sbe16> mode_layer_depths{-30962};       // [depth_mode]
inline constexpr Const<char> text_sonar_reports{-24636}, text_faint_contact{-24618}, text_dots_bearing{-24598},
    text_deg3{-24584}, text_general_quarters{-24578}, text_xo_reports{-24558}, text_our_heading{-24544},
    text_deg4{-24530}, text_our_speed{-24524}, text_knots{-24512}, text_our_depth{-24504}, text_feet2{-24492},
    text_weapons_officer{-24486}, text_loadout{-24458}, text_loadout_line{-24442}, text_tube2{-24416},
    text_colon{-24410}, text_reloads2{-24408}, text_reloads_line{-24400}, text_local_conditions{-24388},
    text_surface_duct{-24368}, text_thermal_at{-24354}, text_feet3{-24334}, text_press_continue2{-24328};  // an aircraft (type 0) that is gone was shot down

// The periscope (display 6).
inline constexpr Const<char> text_bearing_label2{-27332}, text_range_label{-27322};
inline constexpr Const<char> text_wire{-25754}, text_no_wire{-25752}, text_turn_right{-25750},
    text_turn_left{-25748}, text_homing{-25746}, text_jammed{-25736}, text_active{-25726};
inline constexpr Const<char> text_too_deep_launch{-25716}, text_all_active{-25690}, text_no_weapon{-25664},
    text_loaded_sir{-25634}, text_fire{-25620}, text_bearing_word{-25614}, text_range_word{-25606};
// The tactical analysis: a message per situation level (keep_highest's
// record b): text with codes 0 new line, 128 the value as degrees, 129 as
// a number, $ff the end.
inline constexpr Const<be32> analysis_texts{-24088};  // [level]
inline constexpr Const<char> text_tactical_analysis{-24144}, text_press_any_key{-24122};
inline constexpr uint8_t kCodeEnd = 0xFF, kCodeDegrees = 128, kCodeNumber = 129;
inline constexpr int16_t kMaxRunTime = 999, kTubeLoaded = 128, kMaxMissileDepth = 300;
inline constexpr int16_t kHalfOctant = 4096;  // 1/16 turn: rounds an aspect to the nearest eighth
inline constexpr int16_t kReticleImage = 9, kFirstWaveImage = 19, kLastWaveImage = 24;
inline constexpr Const<char> text_too_deep{-27352}, text_bearing_label{-27342};
inline constexpr int16_t kPeriscopeImage = 13, kSkyPen = kPenCyan, kSeaPen = kPenLightBlue;
inline constexpr Const<char> event_texts_a{-26070}, event_texts_b{-26040}, event_texts_c{-26012}, event_texts_d{-25982};
inline constexpr Const<char> text_speed{-26598}, text_amidships{-26584}, text_rudder{-26558}, text_left{-26550},
    text_right{-26544}, text_degrees{-26538}, text_scale_min{-26660}, text_scale_max{-26646},
    text_zoom_in{-26630}, text_zoom_out{-26624}, text_scale{-26616};
inline constexpr int16_t kMessageColour = kPenGreen;
inline constexpr int16_t kNoDepth = 9999;

// Tubes and weapons.
inline constexpr Const<be32> weapon_names{-31704};
inline constexpr Const<char> text_no_reload{-25828}, text_loading{-25808}, text_in_tube{-25798},
    text_sir{-25788}, text_course_range{-26524};
inline constexpr int16_t kLoadTime = 64;

// course_to's results and the tracking state.
inline constexpr int16_t kDetectScore = 147, kCloseRange = 2000;

// Text output: one text style for everything Storm prints.
inline constexpr screen::StyleVar text_style{-32764};
inline constexpr Const<char> blank_text{-24156};     // cleared under shadowed text
inline constexpr Const<char> bearing_text{-24154};   // between a contact's name and its bearing
inline constexpr Const<char> text_pause_status{-26478};

// The status line under the displays.
inline constexpr int16_t kStatusLeft = 73, kStatusTop = 176, kStatusRight = 303, kStatusBottom = 183;
inline constexpr int16_t kStatusCentre = 187, kStatusTextY = 177, kStatusHalfChar = 3;
inline constexpr int16_t kShadowPens = 32;  // a colour's shadow pen is colour + 32

// The simulation's time step and the depths.
inline constexpr int16_t kMaxTrimDepth = 1500;
inline constexpr Const<char> text_active_sonar{-26142}, text_transient{-26106}, text_sonobuoy2{-26096};
inline constexpr int16_t kSonobuoyRange = 3000, kRbuNoise = 304;
inline constexpr Const<char> text_torpedo_approaching{-26190}, text_torpedo_decoyed{-26164};
inline constexpr Const<char> text_enemy_torpedo{-26334}, text_hit_by_weapon{-26320}, text_sunk_by_weapon{-26304},
    text_sunk_by{-26288}, text_damaged_by2{-26278};
inline constexpr Const<char> text_shot_down2{-26264}, text_transient2{-26252}, text_torpedo_decoyed2{-26242},
    text_torpedo_homing{-26226}, text_noise_jamming{-26210};
inline constexpr int16_t kNoTarget = -32767, kHomingRange = -2048, kJammer = 4, kDecoyed = 32;
inline constexpr int16_t kTurnAway = 8192, kMinRunDepth = 25, kMaxRunDepth = 950;
inline constexpr int16_t kLockScore = 50, kNoLock = 1000, kGiveUpRange = 1500, kGiveUpTime = 600;
inline constexpr int16_t kCloseContact = 2048, kNearRange = 4096, kSignalLimit = 199;
inline constexpr int16_t kArrayStraight = 512, kSteadyTurn = 2560;  // 1/128 and 5/128 turn
inline constexpr int16_t kAcceleratedTime = 3;
inline constexpr int16_t kCloseAngle = 2048;     // 1/32 turn

// A projectile (26 bytes; 10 of them).
inline constexpr int16_t kProjectiles = 10;
inline constexpr Const<char> text_unable_unload{-26502};
inline constexpr Const<be32> projectile_names{-31616};  // [kind]
inline constexpr Const<char> text_no{-27208}, text_available{-27204}, text_noisemaker{-27192},
    text_decoy{-27172}, text_knuckle{-27156};
inline constexpr int16_t kNoisemaker = 1, kDecoy = 2, kKnuckle = 3;

// The weapon in the water and its aiming.
inline constexpr int32_t kTrackedWeaponAt = -27124;  // tracked_weapon() (state.cpp)
int16_t& tracked_weapon();  // the weapon the pointer follows

// The aiming cursor on the map.
inline constexpr int16_t kMapLeft = 73, kMapTop = 27, kMapRight = 303, kMapBottom = 167;
inline constexpr int16_t kMapCentreX = 187, kMapCentreY = 97;

// Where each weapon is aimed (12 bytes).
inline constexpr Const<char> text_sonobuoy{-26390};
inline constexpr Const<char> text_min_range{-25600}, text_destroyed_by_ice{-25578};
// The player's weapons (weapon_names: weapon types; weapon_counts by type).
inline constexpr int16_t kMark48 = 1, kSeaLance = 2, kHarpoon = 3, kTomahawk = 4, kTasm = 5, kStinger = 6;
inline constexpr int16_t kWireGuided = kMark48, kMinMissileRange = 750, kOwnClassQuiet = 11, kTorpedoTime = 2000;
inline constexpr Const<char> text_launched{-27106};

// Hitting the sea bed or the ice.
inline constexpr Const<char> text_aground{-26444}, text_iceberg{-26416};
inline constexpr Const<char> text_torpedo_bottom{-26374}, text_torpedo_iceberg{-26354};
inline constexpr int16_t kAground = 64, kIceberg = 32, kNearBottom = 100;

// The map view of the battle log.
inline constexpr Const<sbe16> replay_delay{-30712};  // frames / 8 between replay steps
inline constexpr Const<char> text_zoom{-21136}, text_expand{-21130}, text_to_scale{-21122};
inline constexpr Const<char> text_launches{-21104}, text_damaged_by{-21086}, text_sunk2{-21066};
inline constexpr Const<char> text_launched_sir{-27122};
inline constexpr int16_t kLogEntries = 2047, kLogLastStart = 2002, kLogFirstEnd = 1999;
inline constexpr Const<sbe16> back_bitmap{-30884}, front_bitmap{-30882};
inline constexpr uint16_t kHalfTurn = 0x8000;
inline constexpr int16_t kNoRange = 9999, kNeverSeek = 9999;
inline constexpr int16_t kTorpedoReach = 3000, kMissileShorter = 1024;
inline constexpr int16_t kEnemyTorpedo = 8, kEnemyMissile = 9;

// The battle step.
inline constexpr Const<be32> ship_lists_by_era{-31336};  // [era]: the full lists (32)
inline constexpr int16_t kStartSpread = 8000, kStartRange = 10000, kHeloDistance = 3000;
// The battle's main loop.

// The battle's keys.
inline constexpr Const<char> text_active_sonar2{-27060}, text_on{-27046}, text_off{-27042}, text_active_radar{-27038},
    text_on2{-27024}, text_off2{-27020}, text_too_deep2{-27016}, text_silent_running{-27006},
    text_straight_level{-26974}, text_run_shallow{-26950}, text_wire_broken1{-26938}, text_run_deep{-26926},
    text_wire_broken2{-26916}, text_left_search{-26904}, text_wire_broken3{-26884}, text_right_search{-26872},
    text_wire_broken4{-26850}, text_torpedo_activated{-26838}, text_wire_broken5{-26820}, text_wire_cut{-26808},
    text_track_on{-26784}, text_track_off{-26768}, text_paused{-26750}, text_resume{-26718},
    text_accelerated{-26710}, text_real_time{-26692}, text_sounds_on{-26682}, text_sounds_off{-26672};
inline constexpr int16_t kRunShallow = 50, kRunDeeper = 25, kShadeX = 174, kShadeY = 133;
inline constexpr Const<char> text_range_increasing{-30568}, text_continue_battle{-30536}, text_no_contacts{-30508},
    text_do_we_continue{-30488}, text_mission_over{-30466}, text_view_replay{-30450};
inline constexpr int16_t kRealTime = 6, kQuietTimeout = 500, kQuietStart = 1024, kTickFrames = 20;
// Not the original's: the frames a pass of the battle loop takes in
// accelerated time. The original does not wait there; a pass took as
// long as a 7 MHz 68000 needed for it, a few frames. The C++ needs
// almost none, so without this a battle ran out in a second or two.
// 4 frames: five times real time.
inline constexpr int16_t kAcceleratedTickFrames = 4;
inline constexpr int16_t kQuitKey = 169, kEndKey = 166;  // Alt-Q, Alt-E (input::kKeyAltQ, kKeyAltE)
inline constexpr Const<sbe16> ambient_by_mode{-30954};  // [depth_mode]
inline constexpr Const<sbe16> projectile_kinds{-31244};  // [10]: each slot's kind
inline constexpr int16_t kFormationFast = 3750, kFormationSlow = 1250, kFormationClose = 750, kFormationWide = 2250;
inline constexpr int16_t kSpareFirst = 22, kSpareEnd = 25;  // list entries a battle's other enemies may take
inline constexpr uint16_t kSpareFree = 0x8000;
inline constexpr int16_t kShipList = 25, kShadeLines = 32;

// The battle log's wrap-around.
inline constexpr int16_t kShips = 8;
inline constexpr int16_t kMinLayerDepth = 100, kMaxLayerDepth = 1000;

// keep_highest's two (level, value) records.
inline constexpr Const<char> text_loaded_in_tube{-25780}, text_comma_sir{-25762};

// The panels' labels.
inline constexpr Const<char> text_threats{-27088}, text_weapons{-25948}, text_reloads{-25928},
    text_deg{-24706}, text_knts{-24702}, text_ft{-24696};
inline constexpr int16_t kWeaponTypes = 8, kReloadLines = 5;
inline constexpr int16_t kGridPen3 = kPenCyan, kGridPen5 = kPenDarkRed, kGridPenOther = kPenPlum;
// Not the original's: display 3's grid with the alternative colours
// (mem::alternative_color()), deep sea instead of the bright cyan.
inline constexpr int16_t kGridPen3Alternative = kPenDeepSea;
inline constexpr Const<char> text_tube{-25940}, text_pad{-31260}, text_tasm{-25934};
inline constexpr Const<char> text_cavitating{-24692}, text_quiet{-24690};
inline constexpr int16_t kTubeNameLength = 8;
inline constexpr Const<char> text_select_type{-25404};

// The ship type data base page (from the identification list).
inline constexpr Const<sbe16> type_shapes{-29844}, type_shape_dx{-30432}, type_shape_dy{-30300};  // [type]
inline constexpr Const<char> text_data_base{-25384}, text_max_speed{-25372}, text_knots2{-25356},
    text_weapons_systems{-25350}, text_none4{-25332}, text_homing_torpedos{-25326}, text_asw_missiles{-25306},
    text_rbu_rockets{-25282}, text_asw_helicopter{-25260}, text_sonar_suite{-25244}, text_lf{-25226},
    text_mf{-25222}, text_hf{-25218}, text_hull_active{-25214}, text_hull_passive{-25194},
    text_towed_vd{-25172}, text_none5{-25150}, text_sound_level{-25144}, text_av2{-25130},
    text_very_quiet{-25126}, text_quiet2{-25114}, text_semi_quiet{-25108}, text_semi_noisy{-25096},
    text_noisy{-25084}, text_very_noisy{-25078};

// The sonar conditions display (4).
inline constexpr Const<char> text_transmission{-27312}, text_ambient{-27292}, text_av{-27276}, text_surface{-27272},
    text_duct{-27264}, text_thermal{-27258}, text_layer{-27250}, text_at{-27244}, text_feet{-27240},
    text_depth_mark{-27234}, text_select_contact{-27228};
inline constexpr int16_t kSonarSurface = 49, kSonarRight = 276, kFeetPerPixel = 7, kDeepestBottom = 800;

// The threats panel: enemy weapons heard, nearest first.
inline constexpr Const<char> text_none2{-27080}, text_brg{-27070}, text_rng{-27066};

// The helm panel.
inline constexpr Const<char> text_rudder_left{-24688}, text_rudder_right{-24682}, text_steady{-24676},
    text_to_course{-24668}, text_level{-24660}, text_to{-24652};
inline constexpr Const<char> text_flag_a{-24648}, text_flag_a_off{-24646}, text_flag_t{-24644},
    text_flag_t_off{-24642}, text_flag_r{-24640}, text_flag_r_off{-24638};
inline constexpr int16_t kCourseDigits = 3;  // where "TO XXX " takes the course
inline constexpr int16_t kThreatLines = 5, kLoudWeapon = 192;
inline constexpr Const<char> text_damage{-25064}, text_reports{-25056}, text_dashes2{-25048}, text_none{-25038};
inline constexpr Const<char> text_passive{-25030}, text_sonar{-25022}, text_towed{-25014}, text_array{-25008},
    text_torpedo{-25000}, text_tubes{-24992}, text_prop{-24984}, text_linkage{-24976};

// The torpedo panel's labels.
inline constexpr Const<char> text_torp{-25902}, text_dashes{-25896}, text_time_to{-25886}, text_run{-25878},
    text_time_to2{-25872}, text_pap{-25864}, text_speed2{-25858}, text_kts{-25850}, text_heading{-25846},
    text_deg2{-25838}, text_search{-25834};

// The sea bed: a map of levels (bytes, low 3 bits; rows of 64) in the
// code hunk, from map_base.
inline constexpr Const<uint8_t> sea_bed{-101850};
inline constexpr int16_t kSeaBedRow = 64, kSeaBedRowMask = 0x1FC0;

// The battle's record for the campaign (*battle_record).
struct BattleRecord {
  uint8_t unknown_0[70];
  sbe16 enemy_ships;   // +70
  uint8_t unknown_72[10];
  sbe16 ship_state[7]; // +82: per enemy ship: 0 afloat, 1 damaged, 2 gone
  sbe16 damage;        // +96: the submarine's damage word
  sbe16 tubes[8];      // +98
  sbe16 weapons[7];    // +114: weapons 1-7 left
};

// The pointer moved by Storm (pointer.cpp): an input.device request that
// writes one input event.
struct InputEvent {
  be32 next;
  uint8_t cls, subclass;   // +4
  be16 code, qualifier;    // +6
  sbe16 x, y;              // +10
  be32 secs, micros;       // +14
};
static_assert(sizeof(InputEvent) == 22);
inline constexpr uint16_t kRelativeMouse = 0x8000;
inline constexpr uint8_t kClassRawMouse = 2, kClassPointerPos = 4;
inline constexpr int16_t kPalPointerLines = 10;

// The player's record (common/record.hpp), as a battle reads it: how it
// starts, and what it records.
inline BattleStart* battle_start() { return common::player_record_as<BattleStart>(); }
inline BattleRecord* battle_record() { return common::player_record_as<BattleRecord>(); }

}  // namespace game::storm
