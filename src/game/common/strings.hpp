// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Text helpers of the game's library shared by the modules: numbers in
// fields, the ship names, joining strings.
#pragma once

#include <cstdint>

#include "game/mem/types.hpp"

namespace game::common {

// strings.cpp
const char* format_number(int16_t v, int16_t width);  // v right-aligned in `width` characters (static buffer)
const char* number_field(int32_t v, int16_t width);   // the same for a long
int16_t find_name(const char* name);               // the ship called `name` (kShipNames: none)
inline int16_t find_name(uint32_t name) { return find_name(mem::host_text(name)); }
const char* concat(const char* a, const char* b);           // a then b (static buffer)
const char* format_degrees(int16_t angle);            // "067": an angle (1/65536 turns) in whole degrees
const char* format_thousands(uint16_t v);             // "12,345" (static buffer)
const char* copy_static(const char* text);               // text into a static buffer
inline const char* concat(uint32_t a, uint32_t b) { return concat(mem::host_text(a), mem::host_text(b)); }
inline const char* concat(const char* a, uint32_t b) { return concat(a, mem::host_text(b)); }
inline const char* concat(uint32_t a, const char* b) { return concat(mem::host_text(a), b); }
inline const char* copy_static(uint32_t text) { return copy_static(mem::host_text(text)); }

// The number conversions (strings.cpp)
int16_t format_int(int16_t v, char* out);        // itoa
inline int16_t format_int(int16_t v, uint32_t buffer) { return format_int(v, const_cast<char*>(mem::host_text(buffer))); }
void format_long(int32_t v, char* out);          // ltoa_pow
inline void format_long(int32_t v, uint32_t buffer) { format_long(v, const_cast<char*>(mem::host_text(buffer))); }

// stdio.cpp, startup.cpp, module.cpp
int32_t c_write(int16_t fd, uint32_t buffer, int16_t n);  // write(); -1: failed
void program_exit(int32_t code);                          // does not return
// A module's start-up: the C start, main, the exit. `loader`: the start
// arguments when the loader runs it (as Workbench gave them: its
// directory's lock and its name's address); null: from the CLI (RedStorm).
struct StartArgs {
  uint32_t lock, name;
};
int32_t module_startup(const StartArgs* loader);
void c_exit(int16_t code);
void load_view(uint32_t record);
void save_view(uint32_t record);
void copy_colours(uint32_t record);
void module_start();
void module_exit(int16_t result);
void seed_random();
void screen_to_front();
void open_keyboard();  // keyboard.device (its time: the keys come from the key matrix)
void close_keyboard();

inline constexpr int16_t kShipNames = 28;

}  // namespace game::common
