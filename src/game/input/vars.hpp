// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Input: keys (IDCMP messages and the keyboard matrix), mouse buttons,
// the joystick port, and the menu cursor of RedStart and Red. Records and
// variables. Library offsets are a4-relative in Storm (LibVar); the menu
// state differs per module (PerModule: storm, redstorm, title, redstart,
// red).
#pragma once

#include "game/screen/styles.hpp"
#include "game/ui/state.hpp"

#include "game/mem/constants.hpp"
#include "game/mem/globals.hpp"
#include "game/red/state.hpp"
#include "game/storm/state.hpp"

namespace game::input {

using namespace game::mem;

// intuition.library struct IntuiMessage, the fields used here.
struct IntuiMessage {
  uint8_t message[20];
  be32 im_class;   // +20
  be16 code;       // +24
  be16 qualifier;  // +26
  be32 iaddress;
  sbe16 mouse_x;   // +32
  sbe16 mouse_y;
};
// exec struct IOStdReq (48 bytes).
struct IOStdReq {
  uint8_t node[28];
  be16 command;  // +28
  uint8_t flags;
  int8_t error;
  be32 actual;
  be32 length;   // +36
  be32 data;     // +40
  be32 offset;
};
static_assert(sizeof(IOStdReq) == 48);

inline constexpr uint32_t IDCMP_MOUSEBUTTONS = 8, IDCMP_RAWKEY = 1024;
inline constexpr uint16_t SELECTDOWN = 0x68, MENUDOWN = 0x69, SELECTUP = 0xe8, MENUUP = 0xe9;
inline constexpr uint16_t IECODE_UP_PREFIX = 0x80;

// Library variables.
inline constexpr ConstLib<uint8_t> keys_plain{-11410, Block::Bss};   // [128]: raw key code -> character
inline constexpr ConstLib<uint8_t> keys_shift{-11282, Block::Bss};
inline constexpr ConstLib<uint8_t> keys_alt{-11154, Block::Bss};

inline constexpr int kMatrixKeyBytes = 12, kMatrixQualifiers = 12;
inline constexpr uint8_t kMatrixShift = 0x03, kMatrixAlt = 0x30;

// The characters the key tables (keys_plain, keys_shift, keys_alt, the
// same in every module) give the special keys.
inline constexpr uint16_t kReturn = 13, kSpace = 32;
inline constexpr uint16_t kCursorUp = 128, kCursorLeft = 129, kCursorRight = 130, kCursorDown = 131;  // +4: with Shift
inline constexpr uint16_t kKeyHelp = 136;
inline constexpr uint16_t kKeyF1 = 145, kKeyF2 = 146, kKeyF3 = 147, kKeyF4 = 148, kKeyF5 = 149, kKeyF6 = 150,
                          kKeyF7 = 151, kKeyF8 = 152, kKeyF9 = 153, kKeyF10 = 154;
inline constexpr uint16_t kKeyShiftF1 = 155, kKeyShiftF2 = 156, kKeyShiftF3 = 157, kKeyShiftF4 = 158,
                          kKeyShiftF5 = 159, kKeyShiftF6 = 160, kKeyShiftF7 = 161, kKeyShiftF8 = 162,
                          kKeyShiftF9 = 163, kKeyShiftF10 = 164;
inline constexpr uint16_t kKeyAltA = 165, kKeyAltE = 166, kKeyAltP = 168, kKeyAltQ = 169, kKeyAltR = 170,
                          kKeyAltS = 171, kKeyAltT = 172, kKeyAltV = 173, kKeyAltZ = 174;

// Direction and fire bits (read_stick, controls, the menu).
inline constexpr int16_t kRight = 1, kLeft = 2, kDown = 4, kUp = 8, kFire = 16;

// Per module (a4 offsets; Red's a4+0 is a real variable).
// Return does not fire (Storm: battle.entry_mode; RedStart: a4-30866;
// Red: its a4+0, the key169 flag).
inline constexpr PerModuleMixed<int16_t, sbe16> no_return_fire{
    {0, 0, 0, -30866, 0}, {&storm::battle.entry_mode, nullptr, nullptr, &ui::states[3].no_return_fire, &ui::states[4].quit_key}};

// The menu cursor (RedStart, Red).
struct Bounds {
  sbe16 left, top, right, bottom;
};
struct Point {
  sbe16 x, y;
};
inline constexpr uint32_t kMenuKeyRead = 0x80000000u, kMenuMoved = 0x40000000u, kMenuFire = 0x20000000u;
// The menu's bounds: RedStart's (ui::State; its left is also
// menu_request); Red's are its symbols' style's x, y, width and height
// (a4+54), kept in C++ (screen/styles.hpp).
inline Bounds menu_bounds() {
  if (runtime::module_index() == runtime::kRedModule) return *reinterpret_cast<const Bounds*>(&screen::style_at(mem::a4() + 52).x);
  const int16_t* b = ui::state().menu_bounds;
  Bounds r;
  r.left = b[0];
  r.top = b[1];
  r.right = b[2];
  r.bottom = b[3];
  return r;
}
// set to kRequestKey by the keys (Red's mission.request)
inline constexpr PerModuleMixed<int16_t, sbe16> menu_request{{0, 0, 0, 2, 0},
                                                             {nullptr, nullptr, nullptr, &ui::states[3].menu_bounds[0], &red::state.mission.request}};
// The point steered from (RedStart: a4+168; Red: the submarine).
inline constexpr PerModuleMixed<int16_t, sbe16> steer_ref_x{{0, 0, 0, 168, 0},
                                                            {nullptr, nullptr, nullptr, &ui::states[3].steer_ref_x, &red::state.map_units[4].pos.x}};
inline constexpr PerModuleMixed<int16_t, sbe16> steer_ref_y{{0, 0, 0, 170, 0},
                                                            {nullptr, nullptr, nullptr, &ui::states[3].steer_ref_y, &red::state.map_units[4].pos.y}};
inline constexpr int16_t kRequestKey = 4, kMaxSteer = 3;

// Waiting (RedStart, Red).
inline constexpr int16_t kFramesPerSecond = 60;

// The frame tick (tick.cpp; RedStart, Red). While mouse_steering is set
// (Red: its text pages' map is active) every third tick flips the
// bitmaps (with double_buffer) and waits for the timer.
// RedStart's a4+46/48; Red's phase.short_ranges / alerted.
inline constexpr PerModuleMixed<uint16_t, be16> tick_sound_a{{0, 0, 0, 46, 0},
                                                             {nullptr, nullptr, nullptr, &ui::states[3].tick_sound_a, &red::state.phase.short_ranges}};
inline constexpr PerModuleMixed<uint16_t, be16> tick_sound_b{{0, 0, 0, 48, 0},
                                                             {nullptr, nullptr, nullptr, &ui::states[3].tick_sound_b, &red::state.phase.alerted}};
inline constexpr int16_t kFlipEvery = 3, kTickSoundEvery = 64, kTickSound = 20, kRequestQuit = 3;
inline constexpr uint32_t kTicksPerWait = 2;

}  // namespace game::input
