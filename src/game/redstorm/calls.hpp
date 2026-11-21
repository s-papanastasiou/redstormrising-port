// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// RedStorm (the loader): its routines, by the file that defines them.
#pragma once

#include <string>

#include <cstdint>

#include "game/common/strings.hpp"

namespace game::redstorm {

// loader.cpp
void redstorm_main();
int16_t run_redstart(int16_t start_mode);
int16_t run_red();
int16_t run_storm();
int16_t preload_pictures();
int16_t preload_shapes();
int16_t preload_sounds();
int16_t preload_osg();
int16_t preload_anim_shapes();
int16_t preload_scripts();
void fill_record();
void restore_from_record(uint32_t record);
void load_code();
void load_entry(int16_t m, const char* name, const char* dir);
void free_entry(int16_t m);
void unload_modules();
void quit_game();
void unload_module(uint32_t handle);
uint32_t load_module(const char* name, const char* dir);
std::string path_join(const char* dir, const char* name);

// display.cpp
void open_libraries();
void close_libraries();
void open_display();
void close_display();
void load_indexes();
using common::c_exit;  // $23064 (common/stdio.cpp)

}  // namespace game::redstorm
