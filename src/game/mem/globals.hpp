// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Named module variables for the game code.
//
// Aztec C keeps a module's globals in its small-data area, addressed
// relative to a4 (data hunk + 32766). Every variable the game code uses is
// declared once in a header per module group, with its a4 offset and a
// one-line comment:
//
//   inline constexpr Var<be16> sound_enabled{-30890};      // one module
//   inline constexpr Table<UnitRecord> unit_records{15104}; // records
//   inline constexpr LibVar<be16> cursor_col{-11416, Block::Bss};
//                                         // screen library, all modules
//   inline constexpr PerModule<be16> volume{{-31000, 0, 0, -30990, 0}};
//
// and used as `*sound_enabled = 1`, `unit_records[u].step`, ...
// a4 is the running module's small-data base: its start-up sets it
// (mem::ModuleBase).
#pragma once

#include "game/mem/types.hpp"
#include "runtime/runtime.hpp"

namespace game::mem {

using runtime::Block;

inline uint32_t a4() { return runtime::cpu().a[4]; }

// A variable of one module at a4 + off.
template <typename T>
struct Var {
  int32_t off;
  uint32_t addr() const { return a4() + uint32_t(off); }
  T& operator*() const { return view<T>(addr(), off); }
  T* operator->() const { return &view<T>(addr(), off); }
  // The i-th T from here (the variable is the first element of an array).
  T& operator[](int i) const { return view<T>(addr() + uint32_t(int32_t(i) * int32_t(sizeof(T))), off); }
};

// Consecutive records of type T starting at a4 + off.
template <typename T>
struct Table {
  int32_t off;
  uint32_t addr() const { return a4() + uint32_t(off); }
  T& operator[](int i) const { return view<T>(addr() + uint32_t(int32_t(i) * int32_t(sizeof(T))), off); }
};

// A variable of the screen/input library linked into every module: `off`
// is its offset in Storm, the other modules add their shift for the block
// (see runtime::lib_shift).
template <typename T>
struct LibVar {
  int32_t off;
  Block block;
  uint32_t addr() const {
    const runtime::LibShift& s = runtime::lib_shift();
    return a4() + uint32_t(off + (block == Block::Data ? s.data : s.bss));
  }
  int32_t label() const { return int32_t(addr() - a4()); }
  T& operator*() const { return view<T>(addr(), label()); }
  T* operator->() const { return &view<T>(addr(), label()); }
  // The i-th T from here (the variable is the first element of an array).
  T& operator[](int i) const { return view<T>(addr() + uint32_t(int32_t(i) * int32_t(sizeof(T))), label()); }
};

// A variable whose offset differs irregularly between modules: storm,
// redstorm, title, redstart, red (0 where a module has none).
template <typename T>
struct PerModule {
  int32_t off[5];
  uint32_t addr() const { return a4() + uint32_t(off[runtime::module_index()]); }
  int32_t label() const { return int32_t(addr() - a4()); }
  T& operator*() const { return view<T>(addr(), label()); }
  T* operator->() const { return &view<T>(addr(), label()); }
  // The i-th T from here (the variable is the first element of an array).
  T& operator[](int i) const { return view<T>(addr() + uint32_t(int32_t(i) * int32_t(sizeof(T))), label()); }
};

// A variable of code shared by several modules, kept in C++ by some of
// them: per module (storm, redstorm, title, redstart,
// red) the native field, or else the a4 offset of the emulated one (type
// E, the big-endian form of T).
template <typename T, typename E>
struct PerModuleMixed {
  int32_t off[5];
  T* native[5];

  class Ref {
  public:
    Ref(T* n, uint32_t addr) : n_(n), addr_(addr) {}
    operator T() const { return n_ ? *n_ : T(view<E>(addr_)); }
    Ref& operator=(T v) {
      if (n_) *n_ = v;
      else view<E>(addr_) = v;
      return *this;
    }
    Ref& operator+=(T v) { return *this = T(T(*this) + v); }
    Ref& operator-=(T v) { return *this = T(T(*this) - v); }
    Ref& operator|=(T v) { return *this = T(T(*this) | v); }
    Ref& operator&=(T v) { return *this = T(T(*this) & v); }

  private:
    T* n_;
    uint32_t addr_;
  };
  Ref operator*() const {
    const int m = runtime::module_index();
    return Ref(native[m], native[m] ? 0 : a4() + uint32_t(off[m]));
  }
};

// Data of a module that the original addresses absolutely, not through
// a4 (tables in the data hunk): its address in the tools' layout, per
// module (storm, redstorm, title, redstart, red; 0 where a module has
// none).
template <typename T>
struct PerModuleData {
  uint32_t tool[5];
  uint32_t addr() const { return runtime::maddr(tool[runtime::module_index()]); }
  int32_t label() const { return int32_t(addr() - a4()); }
  T& operator*() const { return view<T>(addr(), label()); }
  T* operator->() const { return &view<T>(addr(), label()); }
  T& operator[](int i) const { return view<T>(addr() + uint32_t(int32_t(i) * int32_t(sizeof(T))), label()); }
};

}  // namespace game::mem
