// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Variables of the shared library routines in src/game/common.
#pragma once

#include "game/mem/constants.hpp"
#include "game/mem/globals.hpp"
#include "game/storm/state.hpp"

namespace game::common {

using namespace game::mem;

// The random number generator's state (storm, -, -, redstart, red).
inline constexpr uint32_t kRandomFeedback = 0x1D872B41u;

// A quarter wave of sines (65 words, 256 = 1) for sin_mul (storm, -, -,
// redstart, red).
inline constexpr ConstPerModule<sbe16> quarter_sines{{-30702, 0, 0, -30706, 0}};
inline constexpr int16_t kQuarterTurn = 64;  // in steps of 1/256 turn

// Angles in 1/65536 turns.
inline constexpr int16_t kAngleQuarter = 16384, kAngleHalf = -32768, kAngleThreeQuarters = -16384;
// bearing's arctangent: angle = r * (kAtanBase - kAtanSlope * |kAtanCentre - r| / 2^14) / 2^14
// for r = 2^14 * min / max.
inline constexpr int16_t kAtanBase = 10240, kAtanSlope = 2816, kAtanCentre = 4915;

// course_to's results (storm, -, -, redstart, red; Red's are both at a4+0).
inline constexpr PerModuleMixed<int16_t, sbe16> course_result_bearing{
    {0, 0, 0, 16206, 0}, {&storm::state.tactical.course_bearing, nullptr, nullptr, nullptr, nullptr}};
inline constexpr PerModuleMixed<int16_t, sbe16> course_result_range{
    {0, 0, 0, 16204, 0}, {&storm::state.tactical.course_range, nullptr, nullptr, nullptr, nullptr}};

// Text helpers (strings.cpp; storm, -, -, redstart, red).
inline constexpr ConstPerModule<char> blanks{{-31296, 0, 0, -31300, 0}};          // 40 spaces
inline constexpr int16_t kBlanks = 40;
inline constexpr int16_t kMaxDigits = 5, kTopPower = 10000;
inline constexpr ConstPerModule<sbe16> int_powers{{-9764, 0, 0, -15460, 12000}};  // [1] = 1 ... [5] = 10000
inline constexpr ConstLib<sbe32> long_powers{-11026, Block::Bss};             // [1] = 1 ... [7]
inline constexpr int kLongDigits = 7;  // digit counts: at most 5
inline constexpr ConstPerModule<be32> ship_names{{-10032, 0, 0, -15024, 11788}};  // [kShipNames]
inline constexpr ConstPerModule<char> degrees_template{{-9768, 0, 0, -15464, 11996}};  // "000"
inline constexpr int kThousandsDigits = 6;
inline constexpr uint8_t kLastControl = 16;  // concat keeps a's characters up to one at or below this

// The C runtime's memory (memory.cpp; storm, redstorm, title, redstart,
// red): blocks from AllocMem in a list (each: +0 next, +4 previous, +8
// its size), carved by malloc from a free list sorted by address (each
// free piece: +0 next, +4 size).
inline constexpr PerModule<be32> free_list{{-9738, 9586, -14798, 0, 0}};  // the free list's head (moved: memory.cpp)
inline constexpr ConstPerModule<sbe16> growth_unit{{-9656, 9668, -14716, 0, 0}};   // the free list grows by multiples
inline constexpr int32_t kMinPiece = 8, kBlockHeader = 12;

// The C start-up and exit (startup.cpp; all five modules: RedStorm is
// started from the CLI, the others as Workbench programs).
inline constexpr PerModuleData<uint8_t> module_a4{{0x3c474, 0x24b54, 0x2d714, 0x3097c, 0x3b174}};  // a4 (small data)
inline constexpr PerModuleData<uint8_t> bss_start{{0x39fc8, 0x27220, 0x29ea0, 0x33fd0, 0x3e944}};  // cleared at start
inline constexpr PerModuleData<uint8_t> bss_end{{0x3ec5c, 0x27528, 0x2eaa0, 0x38260, 0x3fa0c}};
inline constexpr uint32_t kDosLibraryName = 0x201a4;  // "dos.library" after the start-up, in every module
inline constexpr uint32_t kStartSignals = 0x3000;  // cleared at start (SIGBREAKF_CTRL_C | _D)
inline constexpr int32_t kStackMargin = 128;
// The CLI start: the shell left the stack size and a return address on
// the stack, the start-up pushed 13 registers.
inline constexpr uint32_t kCliStartFrame = 60;
inline constexpr int32_t kModeOldFile = 1005;

// stdio (stdio.cpp; all five modules): the buffered files (a list from the
// first, static one) and the table of open file handles.
struct CFile {
  be32 next;          // +0
  be32 ptr;           // +4: the next byte of the buffer
  uint8_t unknown_8[4];
  be32 base;          // +12: the buffer
  uint8_t unknown_16[3];
  uint8_t flags;      // +19: kFileWriting, kFileUnbuffered
  sbe16 fd;           // +20
};
struct FdEntry {
  be16 flags;         // the low byte: open; kFdNoClose
  be32 handle;        // dos file handle
};
static_assert(sizeof(FdEntry) == 6);
inline constexpr ConstPerModule<CFile> stdio_files{{-9728, 9596, -14788, 13572, 13956}};
inline constexpr ConstPerModule<FdEntry> fd_table{{9944, 10436, 4732, 30680, 18316}};  // (BSS no file is ever opened into: all clear)
inline constexpr uint8_t kFileWriting = 2, kFileUnbuffered = 4;
inline constexpr uint16_t kFdNoClose = 0x10, kFdAppend = 0x08;
inline constexpr int16_t kEio = 5, kEbadf = 9, kEinval = 22;
inline constexpr int16_t kSeekSet = 0, kSeekCur = 1, kSeekEnd = 2;  // lseek's modes

// The module hand-over (module.cpp; storm, -, title, redstart, red).
inline constexpr ConstPerModule<char> loader_task_name{{-27468, 0, -27548, -20796, 6364}};  // "RedStormRising"

// I/O requests and the system time (devices.cpp).


}  // namespace game::common
