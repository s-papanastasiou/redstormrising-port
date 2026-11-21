// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Input: the routines of the subsystem.
#pragma once

#include <cstdint>

namespace game::input {

// input.cpp
int16_t getkey();
void flush_keys();
int32_t read_stick();
int16_t key_down();
int32_t controls();

// tick.cpp (RedStart, Red)
void frame_tick();       // the frame tick: the bitmaps' flip, the menu input

// wait.cpp (RedStart, Red)
void wait_released();
void wait_key();
int16_t wait_seconds(int16_t n);

// menu.cpp (RedStart, Red)
void menu_key();
int32_t menu_controls();
void menu_move();

}  // namespace game::input
