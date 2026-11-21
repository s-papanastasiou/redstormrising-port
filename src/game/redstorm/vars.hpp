// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// RedStorm (the loader: it opens the display, preloads the data files by
// the free memory and runs the other modules in turn): variables used by
// the game code. Offsets are a4-relative in RedStorm's small-data area (a4 =
// $24b54). Most of its display and file variables are the screen
// library's own (game/screen/vars.hpp, LibVar).
#pragma once

#include "game/mem/constants.hpp"
#include "game/mem/globals.hpp"
#include "game/common/record.hpp"
#include "game/screen/vars.hpp"

namespace game::redstorm {

using namespace game::mem;
using screen::BitMap;

using common::ModuleRecord;
inline constexpr uint32_t kImageList = 0x23c10, kPointer8x8 = 0x23bdc,
                          kPointer7x7 = 0x23bb8, kPointerNone = 0x23c04;  // tools' addresses

inline constexpr int32_t kMoreMemory = 645000, kLotsMemory = 900000, kMostMemory = 1000000;
inline constexpr int32_t kMinChip = 230000, kMinFree = 370000;

inline constexpr int32_t kAccessRead = -2;
inline constexpr Const<char> path_separator{9572};  // put between a directory and a name
inline constexpr uint8_t kSlash = '/', kBackslash = '\\', kColon = ':';

// The modules: their load handles.
enum Module : int16_t { kRed = 0, kStorm = 1, kRedStart = 2 };
inline constexpr int16_t kRedEnds = 3;  // Red's result: the game is over
// A module's file names (the original keeps a copy per use).
struct ModuleFiles {
  Const<char> load, load_dir;    // the preload with the rest of the code
  Const<char> reload, reload_dir;  // loading it before each run (little memory)
  Const<char> start, start_dir;  // starting it
};
inline constexpr ModuleFiles module_files[3] = {
    {{206}, {220}, {306}, {320}, {322}, {336}},   // kRed: "RedStormB:red"
    {{222}, {238}, {338}, {354}, {356}, {372}},   // kStorm
    {{184}, {204}, {262}, {282}, {284}, {304}},   // kRedStart
};

// Strings.
inline constexpr Const<char> task_name{8}, task_name2{24};  // "RedStormRising"
inline constexpr Const<char> no_chip_text{40}, no_free_text{60}, load_failure_text{240};
inline constexpr Const<char> title_file{80}, title_file2{98}, empty{96}, empty2{114};
inline constexpr Const<char> version_text{116};                                 // "843.02"
inline constexpr Const<char> loading_data_text{124}, loading_more_text{146}, loading_code_text{168};
inline constexpr int16_t kVersionLength = 6, kLoadingDataLength = 20, kLoadingCodeLength = 15;
inline constexpr int16_t kStatusX = 5, kVersionX = 260, kTextY = 2;

// The display.
// The NewScreen and NewWindow (their original layouts, the fields used).
struct NewScreenImage {
  sbe16 left, top, width, height, depth;
  uint8_t detail_pen, block_pen;
  be16 modes, type;
  be32 font, title, gadgets, custom_bitmap;
};
static_assert(sizeof(NewScreenImage) == 32);
struct NewWindowImage {
  sbe16 left, top, width, height;
  uint8_t detail_pen, block_pen;
  be32 idcmp, flags, first_gadget, checkmark, title, screen, bitmap;
  sbe16 min_width, min_height;
  be16 max_width, max_height, type;
};
static_assert(sizeof(NewWindowImage) == 48);
inline constexpr Const<NewScreenImage> new_screen{836};  // (its top: 10 lines lower on PAL)
inline constexpr Const<NewWindowImage> new_window{868};  // (its screen: the one opened)
inline constexpr Const<be16> screen_palette{916};      // [32] for LoadRGB4
inline constexpr Var<screen::BitMapSlot> own_bitmaps{10160};     // [3]: 1 and 2 are the loader's (0: the screen's)
inline constexpr Const<char> graphics_name{1502}, intuition_name{1520}, diskfont_name{1538};
inline constexpr Const<char> font_files[3] = {{1626}, {1676}, {1726}};  // "RedStormA:RedStorm1.fnt" ...
inline constexpr Const<char> no_font_texts[3] = {{1650}, {1700}, {1750}};
inline constexpr Const<char> no_screen_text{1556}, no_window_text{1574}, no_display_memory_text{1592},
    no_planes_text{1776}, no_spbuff_text{1802};
inline constexpr int16_t kPalTop = 10, kScreenViewPort = 44, kScreenRastPort = 84;
inline constexpr int16_t kOwnBitmapsFrom = 1, kBitmaps = 3, kPlanes = 6, kWidth = 320, kHeight = 200;
inline constexpr int16_t kFonts = 4, kFontTextFont = 8 + 54;  // a font segment's TextFont
inline constexpr uint32_t kPlanesBufferBytes = 31500, kSpBuffBytes = 16000;
inline constexpr int32_t kPaletteColours = 32;

// The data files: offset tables (RedStormA:..._index, 400 bytes) and,
// with enough memory, the files themselves (RedStormB:).
inline constexpr Const<char> osg_index{374}, no_osg_index{394}, pics_index{416}, no_pics_index{438},
    ships_index{464}, no_ships_index{486}, sounds_index{510}, no_sounds_index{528}, bms_index{552},
    no_bms_index{572};
inline constexpr Const<char> pics_file{600}, no_pics_file{616}, ships_file{638}, no_ships_file{654},
    sounds_file{678}, no_sounds_file{696}, osg_file{720}, no_osg_file{734}, atbms_file{754},
    no_atbms_file{770}, atscr_file{792}, no_atscr_file{808};
inline constexpr uint32_t kIndexBytes = 400;
inline constexpr uint32_t kPicsBytes = 112000, kShipsBytes = 48000, kSoundsBytes = 98000, kOsgBytes = 48000,
                          kAtbmsBytes = 37000, kAtscrBytes = 27000;
inline constexpr int16_t kSequences = 12;  // in ATSCR

}  // namespace game::redstorm
