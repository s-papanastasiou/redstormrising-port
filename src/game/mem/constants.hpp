// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The modules' constant data: the texts and tables in their data and code
// hunks. When a module is loaded (relocated, patched) a host copy of its
// image is taken; constant data is read from that copy, not from the
// emulated memory. Pointers stored in constant data stay emulated addresses
// (they name other constants) and are resolved through the copy too.
//
//   inline constexpr Const<char> title_text{-1234};   // like Var, read-only
//   constant<Menu>(addr), const_text(addr)            // through a stored address
//
// Writing through a Const is a compile error. In the trace run
// (RSR_TRACE=1) each read is compared with the emulated memory: a byte that
// changed there prints "CONST CHANGED" (the gate fails on it), so a table
// the game does write is never taken for a constant.
#pragma once

#include "game/mem/globals.hpp"

namespace game::mem {

namespace constants {
// The host copy of [addr, addr + size) (in a loaded module's image; else
// the program stops).
const uint8_t* bytes(uint32_t addr, uint32_t size);
// The C string at addr, in the copy.
const char* text(uint32_t addr);
// addr lies in a loaded module's image.
bool contains(uint32_t addr);
}  // namespace constants

template <typename T>
const T& constant(uint32_t addr) {
  static_assert(alignof(T) == 1 && std::is_trivially_copyable_v<T>);
  return *reinterpret_cast<const T*>(constants::bytes(addr, sizeof(T)));
}
inline const char* const_text(uint32_t addr) { return constants::text(addr); }
// n Ts from addr.
template <typename T>
const T* constant_array(uint32_t addr, int n) {
  static_assert(alignof(T) == 1 && std::is_trivially_copyable_v<T>);
  return reinterpret_cast<const T*>(constants::bytes(addr, uint32_t(n) * sizeof(T)));
}

// A stored address of constant data (a field of state that names a
// table, as a menu's entry list): read through the copy.
template <typename T>
struct ConstPtr {
  be32 addr;
  ConstPtr() = default;
  explicit ConstPtr(uint32_t a) { addr = a; }
  const T& operator*() const { return constant<T>(addr); }
  const T* operator->() const { return &constant<T>(addr); }
  const T& operator[](int i) const { return constant<T>(addr + uint32_t(i * int32_t(sizeof(T)))); }
  ConstPtr& operator=(uint32_t a) {
    addr = a;
    return *this;
  }
  explicit operator bool() const { return addr.get() != 0; }
};
static_assert(sizeof(ConstPtr<char>) == 4);

// A constant of one module at a4 + off.
template <typename T>
struct Const {
  int32_t off;
  uint32_t addr() const { return a4() + uint32_t(off); }
  const T& operator*() const { return constant<T>(addr()); }
  const T* operator->() const { return &constant<T>(addr()); }
  const T& operator[](int i) const { return constant<T>(addr() + uint32_t(i * int32_t(sizeof(T)))); }
  const T* array(int n) const { return constant_array<T>(addr(), n); }  // the first n
  // A list ended by an entry for which last(entry) is true: the entries
  // up to it (the original reads exactly these).
  template <typename F>
  const T* until(F last) const {
    int n = 1;
    while (!last((*this)[n - 1])) ++n;
    return array(n);
  }
  // A text constant as a host string.
  const char* str() const requires std::is_same_v<T, char> { return const_text(addr()); }
};

// A constant of the library linked into every module (see LibVar).
template <typename T>
struct ConstLib {
  int32_t off;
  Block block;
  uint32_t addr() const { return LibVar<T>{off, block}.addr(); }
  const T& operator*() const { return constant<T>(addr()); }
  const T* operator->() const { return &constant<T>(addr()); }
  const T& operator[](int i) const { return constant<T>(addr() + uint32_t(i * int32_t(sizeof(T)))); }
  const T* array(int n) const { return constant_array<T>(addr(), n); }  // the first n
  const char* str() const requires std::is_same_v<T, char> { return const_text(addr()); }
};

// A constant at irregular a4 offsets per module (see PerModule).
template <typename T>
struct ConstPerModule {
  int32_t off[5];
  uint32_t addr() const { return a4() + uint32_t(off[runtime::module_index()]); }
  const T& operator*() const { return constant<T>(addr()); }
  const T* operator->() const { return &constant<T>(addr()); }
  const T& operator[](int i) const { return constant<T>(addr() + uint32_t(i * int32_t(sizeof(T)))); }
  const T* array(int n) const { return constant_array<T>(addr(), n); }  // the first n
  const char* str() const requires std::is_same_v<T, char> { return const_text(addr()); }
};

// A constant the original addresses absolutely (see PerModuleData).
template <typename T>
struct ConstData {
  uint32_t tool[5];
  uint32_t addr() const { return runtime::maddr(tool[runtime::module_index()]); }
  const T& operator*() const { return constant<T>(addr()); }
  const T* operator->() const { return &constant<T>(addr()); }
  const T& operator[](int i) const { return constant<T>(addr() + uint32_t(i * int32_t(sizeof(T)))); }
  const T* array(int n) const { return constant_array<T>(addr(), n); }  // the first n
  const char* str() const requires std::is_same_v<T, char> { return const_text(addr()); }
};

}  // namespace game::mem
