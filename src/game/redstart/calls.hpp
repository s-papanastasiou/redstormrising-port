// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// RedStart: the module's routines, by the file that defines them.
#pragma once

#include <cstdint>

#include "game/mem/types.hpp"

#include "game/common/strings.hpp"
#include "game/red/calls.hpp"

namespace game::redstart {

struct Span;
struct SequencePage;

using red::text_n_ptr;
using common::concat;
using common::copy_static;
using common::format_thousands;

// saves.cpp
int16_t load_file(const char* name, uint16_t size);
void save_line(int16_t row);
void save_game();
int16_t recall_game();
void medal_row(int16_t medals);
void view_honor_roll();
void add_to_honor_roll();
const char* flag_names(int16_t medals);
int16_t save_disk_ok();

// menus.cpp
int16_t main_menu();
int16_t identification_test();
void choose_rank();
void show_assignment();
void choose_difficulty();
void choose_scenario();
void choose_era();
void enter_name();
void wrong_answer();
void intro_sequence();
void intro_page(int16_t n);
void store_choices();
void input_text(char* buffer, int16_t length, int16_t column, int16_t row);
int16_t start_main(int16_t mode);
void draw_spans(const mem::Const<Span>& spans, int16_t colour);
uint8_t page_sequence(const mem::Const<SequencePage>& pages, int16_t count, int16_t seconds, uint8_t control_only);

}  // namespace game::redstart
