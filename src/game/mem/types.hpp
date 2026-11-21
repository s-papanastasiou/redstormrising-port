// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Typed access to the emulated memory, for what the game still keeps
// there (records in the original's byte layout, file data, the
// constants' images).
//
// These types let plain structs be laid over it: a record is declared
// once, with its fields named and its size asserted.
//
//   be16 / sbe16 / be32 / sbe32   unsigned / signed big-endian fields
//   uint8_t / int8_t              byte fields (no conversion needed)
//   Ptr<T>                        a stored 32-bit emulated address of a T
//   view<T>(addr)                 the T at an emulated address
//
// All of them have alignment 1, so a struct of them has no padding and its
// layout is exactly the record's.
#pragma once

#include "amiga/machine.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>

namespace runtime { extern amiga::Machine* g_machine; }

namespace game::mem {

// The host bytes of [addr, addr + n): RAM, a module's image or an arena
// block (items 4e.1, 4e.5c). The program stops for anything else.
inline uint8_t* host_bytes(uint32_t addr, uint32_t n) { return runtime::g_machine->bytes(addr, n); }
// The byte at emulated address `a`.
inline uint8_t& byte_at(uint32_t a) { return *host_bytes(a, 1); }

// RSR_STATE_TRACE=FILE: every access of the game code to a module's small-data
// area is counted by the declaration it went through (its a4 offset) or
// by kind (a raw pointer, a byte copy, a string), and written to FILE at
// exit. tools/analysis/statemap.py reads it (finding
// the variables that overlap before state moves out of the memory image).
namespace trace {
inline constexpr int32_t kRaw = -0x40000000, kBytes = kRaw + 1, kString = kRaw + 2;
extern bool on;
void note(uint32_t addr, uint32_t size, int32_t label);
}  // namespace trace

namespace detail {
template <typename V, int N>
struct BigEndian {
  static_assert(N == 2 || N == 4);
  uint8_t b[N];

  V get() const {
    uint32_t v = 0;
    for (int i = 0; i < N; ++i) v = v << 8 | b[i];
    return V(v);
  }
  void set(V v) {
    uint32_t u = uint32_t(v);
    for (int i = N - 1; i >= 0; --i) { b[i] = uint8_t(u); u >>= 8; }
  }
  operator V() const { return get(); }
  BigEndian& operator=(V v) { set(v); return *this; }
  BigEndian& operator+=(V v) { set(V(get() + v)); return *this; }
  BigEndian& operator-=(V v) { set(V(get() - v)); return *this; }
  BigEndian& operator|=(V v) { set(V(get() | v)); return *this; }
  BigEndian& operator&=(V v) { set(V(get() & v)); return *this; }
};
}  // namespace detail

using be16 = detail::BigEndian<uint16_t, 2>;
using sbe16 = detail::BigEndian<int16_t, 2>;
using be32 = detail::BigEndian<uint32_t, 4>;
using sbe32 = detail::BigEndian<int32_t, 4>;

// The T at emulated address `addr`.
// `label`: the declaration it is reached through (trace).
template <typename T>
T& view(uint32_t addr, int32_t label = trace::kRaw) {
  static_assert(alignof(T) == 1, "build records from be16/be32/uint8_t only");
  static_assert(std::is_trivially_copyable_v<T>);
  if (trace::on) [[unlikely]] trace::note(addr, sizeof(T), label);
  return *reinterpret_cast<T*>(host_bytes(addr, sizeof(T)));
}

// A pointer stored in emulated memory (a big-endian 32-bit address).
template <typename T>
struct Ptr {
  be32 addr;

  T* get() const { return addr.get() ? &view<T>(addr.get()) : nullptr; }
  T* operator->() const { return get(); }
  T& operator*() const { return view<T>(addr.get()); }
  // A U at a byte offset from the address (records of mixed layout).
  template <typename U> U& at(uint32_t byte_offset) const { return view<U>(addr.get() + byte_offset); }
  T& operator[](int32_t i) const { return view<T>(addr.get() + uint32_t(i * int32_t(sizeof(T)))); }
  explicit operator bool() const { return addr.get() != 0; }
  bool operator==(uint32_t a) const { return addr.get() == a; }
  Ptr& operator=(uint32_t a) { addr = a; return *this; }
};

// A Ptr<T> holding emulated address `addr` (an address passed as an
// argument).
template <typename T>
Ptr<T> ptr_to(uint32_t addr) {
  Ptr<T> p;
  p = addr;
  return p;
}

// The C string at emulated address `addr`, as host characters: for code
// that takes host strings.
inline const char* host_text(uint32_t addr) { return reinterpret_cast<const char*>(host_bytes(addr, 1)); }

// The length of the C string at emulated address `addr`.
inline int32_t string_length(uint32_t addr) {
  int32_t n = 0;
  while (byte_at(addr + uint32_t(n))) ++n;
  if (trace::on) [[unlikely]] trace::note(addr, uint32_t(n + 1), trace::kString);
  return n;
}

// The C string at emulated address `addr`, as a host string.
inline std::string host_string(uint32_t addr) { return std::string(host_text(addr)); }

// C string operations on emulated memory (the game's strcpy, strcat,
// strcmp, strncpy).
inline void string_copy(uint32_t dst, uint32_t src) {
  if (trace::on) [[unlikely]] {
    const uint32_t n = uint32_t(string_length(src)) + 1;
    trace::note(dst, n, trace::kString);
  }
  do byte_at(dst++) = byte_at(src);
  while (byte_at(src++));
}
// Copies at most n bytes and pads with NULs after the terminator (strncpy).
inline void string_copy_n(uint32_t dst, uint32_t src, int32_t n) {
  if (trace::on) [[unlikely]] { trace::note(dst, uint32_t(n), trace::kString); trace::note(src, uint32_t(n), trace::kString); }
  int32_t i = 0;
  for (; i < n; ++i) if (!(byte_at(dst + uint32_t(i)) = byte_at(src + uint32_t(i)))) break;
  for (; i < n; ++i) byte_at(dst + uint32_t(i)) = 0;
}
inline void string_append(uint32_t dst, uint32_t src) { string_copy(dst + uint32_t(string_length(dst)), src); }
// strncpy from a host string.
inline void string_copy_n(uint32_t dst, const char* src, int32_t n) {
  if (trace::on) [[unlikely]] trace::note(dst, uint32_t(n), trace::kString);
  int32_t i = 0;
  for (; i < n; ++i) if (!(byte_at(dst + uint32_t(i)) = uint8_t(src[i]))) break;
  for (; i < n; ++i) byte_at(dst + uint32_t(i)) = 0;
}
// The same with a host string as the source.
inline void string_copy(uint32_t dst, const char* src) {
  uint32_t n = 0;
  do byte_at(dst + n) = uint8_t(src[n]);
  while (src[n++]);
  if (trace::on) [[unlikely]] trace::note(dst, n, trace::kString);
}
inline void string_append(uint32_t dst, const char* src) { string_copy(dst + uint32_t(string_length(dst)), src); }
// Into a host buffer, from an emulated string.
inline void string_copy(char* dst, uint32_t src) { std::strcpy(dst, host_text(src)); }
inline void string_append(char* dst, uint32_t src) { std::strcat(dst, host_text(src)); }
inline void string_copy(char* dst, const char* src) { std::strcpy(dst, src); }
inline void string_append(char* dst, const char* src) { std::strcat(dst, src); }
inline bool string_equal(uint32_t a, uint32_t b) {
  if (trace::on) [[unlikely]] { string_length(a); string_length(b); }
  for (;; ++a, ++b) {
    if (byte_at(a) != byte_at(b)) return false;
    if (!byte_at(a)) return true;
  }
}
inline bool string_equal(uint32_t a, const char* b);
inline bool string_equal(const char* a, uint32_t b) { return string_equal(b, a); }
inline bool string_equal(uint32_t a, const char* b) {
  if (trace::on) [[unlikely]] string_length(a);
  for (;; ++a, ++b) {
    if (byte_at(a) != uint8_t(*b)) return false;
    if (!*b) return true;
  }
}
// Copies n bytes (the source has no terminator among them).
inline void bytes_copy(uint32_t dst, uint32_t src, int32_t n) {
  if (trace::on) [[unlikely]] { trace::note(dst, uint32_t(n), trace::kBytes); trace::note(src, uint32_t(n), trace::kBytes); }
  if (n <= 0) return;
  const uint8_t* from = host_bytes(src, uint32_t(n));
  uint8_t* to = host_bytes(dst, uint32_t(n));
  for (int32_t i = 0; i < n; ++i) to[i] = from[i];
}
// Copies n bytes between emulated memory and a host object (file data
// into and out of native records).
inline void bytes_copy(uint8_t* dst, uint32_t src, int32_t n) {
  if (trace::on) [[unlikely]] trace::note(src, uint32_t(n), trace::kBytes);
  if (n <= 0) return;
  const uint8_t* from = host_bytes(src, uint32_t(n));
  for (int32_t i = 0; i < n; ++i) dst[i] = from[i];
}
inline void bytes_copy(uint32_t dst, const uint8_t* src, int32_t n) {
  if (trace::on) [[unlikely]] trace::note(dst, uint32_t(n), trace::kBytes);
  if (n <= 0) return;
  uint8_t* to = host_bytes(dst, uint32_t(n));
  for (int32_t i = 0; i < n; ++i) to[i] = src[i];
}
// Clears a module's BSS at its start (not traced: the part that moved out
// stays there, unused).
inline void clear_bss(uint32_t dst, int32_t n) {
  if (n > 0) std::memset(host_bytes(dst, uint32_t(n)), 0, size_t(n));
}
// Sets n bytes to v.
inline void bytes_fill(uint32_t dst, uint8_t v, int32_t n) {
  if (trace::on) [[unlikely]] trace::note(dst, uint32_t(n), trace::kBytes);
  if (n <= 0) return;
  uint8_t* to = host_bytes(dst, uint32_t(n));
  for (int32_t i = 0; i < n; ++i) to[i] = v;
}

}  // namespace game::mem
