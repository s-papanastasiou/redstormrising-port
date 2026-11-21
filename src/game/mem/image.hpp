// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The byte image of native state: how a C++ struct
// is laid out where it meets emulated memory (the player's record, which
// goes to Storm and to the save files). The original's layout: big-endian
// fields in order, no padding.
//
// A struct takes part by listing its fields once, in order:
//
//   IMAGE_FIELDS(Date, s.year, s.month, s.day, s.hour, s.minute);
//
// Integers, Ptr (an emulated address), arrays of them and such structs
// nest freely.
//
//   image::read(bytes, value)    from the image
//   image::write(bytes, value)   into it
//   image::size<T>()             its length (static_assert it)
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "game/mem/constants.hpp"
#include "game/mem/types.hpp"

namespace game::mem::image {

namespace detail {
template <typename T> struct IsPtr : std::false_type {};
template <typename U> struct IsPtr<Ptr<U>> : std::true_type {};
template <typename U> struct IsPtr<ConstPtr<U>> : std::true_type {};

template <typename T>
constexpr void count(uint32_t& n, const T& v) {
  if constexpr (std::is_integral_v<T>) n += sizeof(T);
  else if constexpr (IsPtr<T>::value) n += 4;
  else if constexpr (std::is_array_v<T>) for (const auto& e : v) count(n, e);
  else fields(v, [&](const auto& f) { count(n, f); });
}
template <typename T>
void put(uint8_t*& p, const T& v) {
  if constexpr (std::is_integral_v<T>) {
    for (int i = int(sizeof(T)) - 1; i >= 0; --i) *p++ = uint8_t(uint64_t(v) >> (8 * i));
  } else if constexpr (IsPtr<T>::value) {
    put(p, v.addr.get());
  } else if constexpr (std::is_array_v<T>) {
    for (const auto& e : v) put(p, e);
  } else {
    fields(v, [&](const auto& f) { put(p, f); });
  }
}
template <typename T>
void get(const uint8_t*& p, T& v) {
  if constexpr (std::is_integral_v<T>) {
    uint64_t u = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) u = u << 8 | *p++;
    v = T(u);
  } else if constexpr (IsPtr<T>::value) {
    uint32_t a;
    get(p, a);
    v = a;
  } else if constexpr (std::is_array_v<T>) {
    for (auto& e : v) get(p, e);
  } else {
    fields(v, [&](auto& f) { get(p, f); });
  }
}
}  // namespace detail

template <typename T>
constexpr uint32_t size() {
  uint32_t n = 0;
  detail::count(n, T{});
  return n;
}
template <typename T>
void write(uint8_t* bytes, const T& v) { detail::put(bytes, v); }
template <typename T>
void read(const uint8_t* bytes, T& v) { detail::get(bytes, v); }

}  // namespace game::mem::image

// The fields of `Type` in image order (`s` names the object).
#define IMAGE_FIELDS(Type, ...)                                                   \
  template <typename S, typename F>                                             \
    requires std::is_same_v<std::remove_const_t<S>, Type>                       \
  constexpr void fields(S& s, F f) {                                            \
    [&](auto&... x) { (f(x), ...); }(__VA_ARGS__);                              \
  }
