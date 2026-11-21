// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// RedStart (the start module: the player's record, saved games, the hall
// of fame, the choice of campaign): variables used by the game code. Offsets
// are a4-relative in RedStart's small-data area (a4 = $3097c).
#pragma once

#include "game/common/state.hpp"

#include "game/mem/constants.hpp"
#include "game/mem/globals.hpp"
#include "game/red/vars.hpp"

namespace game::redstart {

using namespace game::mem;
using red::PlayerRecord;


// A saved game's entry in the index file (80 bytes; 5 of them).
struct SaveEntry {
  be16 rank;           // +0: the rank's text number (0: free)
  char name[32];       // +2
  char ship[32];       // +34
  sbe16 level, month, day;  // +66
  uint8_t unknown_72[8];
};
static_assert(sizeof(SaveEntry) == 80 && alignof(SaveEntry) == 1);
inline constexpr int16_t kSaveSlots = 5, kSaveIndexBytes = 400, kSaveGameBytes = 500;


// A horizontal line of a page's decoration (draw_spans; a list ends with
// x0 = y = 0).
struct Span {
  sbe16 x0, y, x1;
  sbe16 unused;
};
static_assert(sizeof(Span) == 8);

// A page of a timed sequence (20 bytes): menu_page blocks, its picture's
// text number at +14.
struct SequencePage {
  uint8_t blocks[14];
  be16 picture;
  uint8_t unknown_16[4];
};
static_assert(sizeof(SequencePage) == 20);

// An entry of the honor roll (40 bytes; 5 of them).
struct FameEntry {
  be16 rank;           // +0: the rank's text number (0: none)
  char name[32];       // +2
  be16 score;          // +34
  be16 final_rank;     // +36: its text number
  be16 medals;         // +38
};
static_assert(sizeof(FameEntry) == 40 && alignof(FameEntry) == 1);
inline constexpr int16_t kFameEntries = 5, kFameBytes = 200;
inline constexpr Const<char> fames_file{-29912}, fames_file2{-29880}, fames_file3{-29860}, text_usn2{-29892},
    text_er{-29884}, games_file3{-30044}, game_file_prefix2{-30024};
inline constexpr Const<red::PageBlock> page_honor_roll{-29938}, page_recall{-30086};
inline constexpr Const<red::RowMenu> menu_recall{-30060};
inline constexpr Const<sbe16> medal_row_shapes{-29950};  // [6]
inline constexpr Const<char> save_volume{-15842}, insert_save_disk_text{-15828};  // "RedStormSave:"
inline constexpr Const<be16> honor_palette{-16852};
inline constexpr Const<char> medal_base_text{-30006};   // flag_names' first part
inline constexpr Const<be32> medal_names{-29974};  // [6]: by medal bit
inline constexpr int16_t kMedalBits = 6;
inline constexpr int16_t kRecalled = 1, kNotRecalled = 8;

// The first menu (start, resume, save, recall, view and add to the honor
// roll) and the identification test.
inline constexpr Const<be16> main_palette{-16916};
inline constexpr Const<red::PageBlock> page_main{-30506};
inline constexpr Const<red::RowMenu> menu_main{-30422};
inline constexpr Const<red::PageBlock> page_ident{-29722};
inline constexpr Const<Span> spans_ident{-29668};
inline constexpr Const<red::RowMenu> menu_ident{-29684};
inline constexpr Const<sbe16> ident_shapes{-29762};   // [ship]
enum MainChoice : uint8_t { kStart = 1, kResume = 2, kSave = 3, kRecall = 4, kViewRoll = 5, kAddRoll = 6 };
inline constexpr int16_t kIdentShips = 20, kIdentSkip = 3;

// The new player's rank, difficulty and scenario.
inline constexpr Const<uint8_t> rank_init{-29566};       // [5]: the new player's rank bits
inline constexpr Const<uint8_t> rank_bits{-29561};       // [8]
inline constexpr Const<uint8_t> rank_table{-29510};      // [era * 4 + row]: 128 not offered, else odds
inline constexpr Const<sbe16> rank_texts{-29404};        // [row]: text numbers
inline constexpr Const<red::PageBlock> page_rank{-29552};
inline constexpr Const<Span> spans_rank{-29494};
inline constexpr Const<red::RowMenu> menu_rank{-29526};
inline constexpr Const<red::PageBlock> page_assignment{-29442};
inline constexpr Const<Span> spans_assignment{-29394};
inline constexpr Const<char> text_usn3{-29382}, text_uss2{-29376};
inline constexpr Const<red::PageBlock> page_difficulty{-29370};
inline constexpr Const<Span> spans_difficulty{-29328};
inline constexpr Const<red::RowMenu> menu_difficulty{-29344};
inline constexpr Const<be16> difficulty_palette{-30576};
inline constexpr uint32_t kEmptyHook = 0x24636, kZeroHook = 0x243d6;  // code: rts / moveq #0,d0; rts
inline constexpr Const<red::PageBlock> page_scenario{-29292};
inline constexpr Const<Span> spans_scenario{-29186};
inline constexpr Const<red::RowMenu> menu_scenario{-29218};
inline constexpr Const<uint8_t> scenario_codes{-29202};  // [row]

// Set up by start_main (RedStart's main routine).
inline constexpr screen::StyleVar page_style{-32768};  // red::page_text_style points here
inline constexpr screen::StyleVar row_style{-15592};   // red::row_colours points here
inline constexpr screen::StyleVar window_text_style{-15648};  // a text style (its pen: the save and honor roll lines)
inline constexpr uint32_t kTextList = 0x2a3a8;          // RedStart's texts (data): red::text_list
inline constexpr int16_t kStartEffect = 4, kNamePageTop = 96, kNewEndingRank = 26, kDemoScenario = 9;
inline constexpr int16_t kRankRows = 4, kRankRandom = 5, kRankShape = 40, kScenarioShape = 46, kDifficultyShape = 58;
inline constexpr int16_t kRankText = 170, kRankCodeTexts = 176;
inline constexpr Const<red::PageBlock> page_era{-29840};
inline constexpr Const<Span> spans_era{-29798};
inline constexpr Const<red::RowMenu> menu_era{-29814};
inline constexpr Const<red::PageBlock> page_name{-29656}, page_name_done{-29642}, page_wrong_answer{-29592};
inline constexpr Const<Span> spans_name{-29604};
inline constexpr Const<SequencePage> intro_pages{-29158};
inline constexpr Const<red::PageBlock> page_intro{-29058};
inline constexpr Const<sbe16> intro_pictures{-29044};    // [scenario]
inline constexpr int16_t kNameLength = 16, kNameX = 12, kNameY = 20;

inline constexpr Const<be32> month_texts{-30192}, level_texts{-30120};  // [month], [level]
inline constexpr Const<char> text_usn{-30104}, text_uss{-30096}, text_comma{-30090};
inline constexpr Const<char> games_file{-30322}, games_file2{-30302}, game_file_prefix{-30282};
// Menu pages and row menus.
inline constexpr Const<red::PageBlock> page_saves_full{-30406}, page_save{-30380};
inline constexpr Const<red::RowMenu> menu_saves_full{-30354}, menu_save{-30338};

// The record passed between modules (common/record.hpp).
inline PlayerRecord* player() { return common::player_record_as<PlayerRecord>(); }

}  // namespace game::redstart
