// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// BitMaps for the game code: host
// objects (amiga::BitMap) named by the address where the original kept
// their 40 bytes. BitMapRef replaces a stored Ptr<BitMap>; a declaration
// of BitMaps in a module's data is of BitMapSlot (40 bytes) and reached
// through bitmap(address).
#pragma once

#include "amiga/bitmap.hpp"
#include "amiga/hle.hpp"
#include "amiga/rastport.hpp"
#include "game/mem/types.hpp"

namespace game::screen {

using BitMap = amiga::BitMap;
using amiga::kBitMapBytes;

inline BitMap& bitmap(uint32_t handle) { return runtime::g_machine->bitmap(handle); }
// (bitmap.cpp: a BitMap's 40 bytes in emulated memory are a moved address
// range while it is a host object: the trace run reports an access.)

struct BitMapRef {
  mem::be32 addr;
  BitMap* operator->() const { return &bitmap(addr); }
  BitMap& operator*() const { return bitmap(addr); }
  BitMapRef& operator=(uint32_t a) {
    addr = a;
    return *this;
  }
  explicit operator bool() const { return addr.get() != 0; }
};
static_assert(sizeof(BitMapRef) == 4);

// RastPorts the same way (amiga::RastPort; RastPortRef for a stored
// pointer).
using RastPort = amiga::RastPort;
inline RastPort& rastport(uint32_t handle) { return runtime::g_machine->rastport(handle); }
struct RastPortRef {
  mem::be32 addr;
  RastPort* operator->() const { return &rastport(addr); }
  RastPort& operator*() const { return rastport(addr); }
  RastPortRef& operator=(uint32_t a) {
    addr = a;
    return *this;
  }
};
static_assert(sizeof(RastPortRef) == 4);

// Intuition's screens and windows (amiga::Screen, amiga::Window: item
// 4e.4c) by handle; ScreenRef for a stored pointer to the screen.
using Screen = amiga::Screen;
using Window = amiga::Window;
inline Screen& screen_at_handle(uint32_t handle) { return runtime::g_machine->hle().screen(handle); }
inline Window& window_at_handle(uint32_t handle) { return runtime::g_machine->hle().window(handle); }
struct ScreenRef {
  mem::be32 addr;
  Screen* operator->() const { return &screen_at_handle(addr); }
  Screen& operator*() const { return screen_at_handle(addr); }
  ScreenRef& operator=(uint32_t a) {
    addr = a;
    return *this;
  }
  explicit operator bool() const { return addr.get() != 0; }
};
static_assert(sizeof(ScreenRef) == 4);

// The place of a BitMap in a module's data (for its address only).
struct BitMapSlot {
  uint8_t bytes[kBitMapBytes];
};


}  // namespace game::screen
