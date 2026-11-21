// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red: the module's routines, by the file that defines them.
#pragma once

#include "game/common/math.hpp"
#include "game/input/calls.hpp"
#include "game/red/state.hpp"

#include <cstdint>

#include "game/mem/constants.hpp"
#include "game/mem/types.hpp"

namespace game::screen {
struct TextStyle;
TextStyle& style_at(uint32_t handle);
}  // namespace game::screen

namespace game::red {

struct PageBlock;
struct RowMenu;
struct MenuEntry;

int16_t red_main();  // turn.cpp: the campaign until a battle or its end
void delay(int32_t ticks);  // pages.cpp: dos Delay (1/50 s)

using common::rnd;
using input::wait_seconds;

int16_t unit_step(int16_t unit);  // units.cpp
void effect_none();               // units.cpp
// movement.cpp
int16_t map_distance(const Position& from, const Position& to);
int16_t place_distance(int16_t a, int16_t b);
int16_t unit_place_distance(int16_t unit, int16_t place);
int16_t unit_distance(int16_t a, int16_t b);
int16_t dest_distance(int16_t unit);
int16_t passed(int16_t unit, int16_t from, int16_t to);
int16_t unit_heading(int16_t unit);
void set_velocity(int16_t unit);
void move_unit(int16_t unit);
void set_heading_bits(int16_t unit);
void set_speed(int16_t unit, int16_t speed);
void order_start(int16_t unit);
void order_next(int16_t unit);
void init_headings();
int16_t nearest_of(int16_t from, mem::ConstPtr<int8_t> list, int16_t mask);
void unit_frame(int16_t unit, int16_t frame);
void set_ranges(int16_t player, int16_t enemy);
void scroll_speed();
void scroll_input();  // map.cpp
// fronts.cpp
int16_t map_pixel(int16_t x, int16_t y);
void plot_all(int16_t x, int16_t y, int16_t colour);
void fronts_step();
void front_advance(int16_t front);
// routes.cpp
int16_t at_sea(int16_t place);
int16_t patrol(int16_t unit);
int16_t pick_bit(int16_t mask, int16_t n);
void next_target(int16_t unit);
void next_place(int16_t unit);
void unit3_destination();
void unit6_route();
int16_t wander(int16_t dir);
void order(int16_t unit);
// campaign.cpp
void input_bits();
int16_t group_event(int16_t g);
// campaign.cpp
void group_speed(int16_t g);
// fronts.cpp (more)
void save_fronts();
void fronts_turn();
// campaign.cpp (more)
void battle_result(int16_t won);
void score_factor();
// pages.cpp (more)
void briefing_message();
void award_medal();
void news_report();
void phase_values();
void new_phase();
void update_targets();
// groups.cpp
void start_group(int16_t g);
// clock.cpp
void mark_redraw();
int16_t reinforce_chance();
void put_char(int16_t ch);
int16_t order_code(int16_t v);
void advance_time(int16_t days, int16_t hours, int16_t minutes);
// menu.cpp
void draw_entry(const screen::TextStyle& style, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t a, int16_t b);
void add_row(const RowMenu& menu, int16_t row);
void menu_page(const PageBlock* blocks);  // up to a block of count 0
void menu_page(const mem::Const<PageBlock>& page);  // a menu page in the constant data
int16_t choose_row(const RowMenu& menu, int16_t start);
void window_char(int16_t ch);
// texts.cpp
void unpack_text(int16_t newline);
const char* copy_unpack(uint32_t packed);
const char* message(int16_t n);
const char* message2(int16_t n);
void text_n(int16_t n);
const char* text_n_ptr(int16_t n);
void draw_to_window(int16_t bitmap);
void recolour_rect(const amiga::BitMap& bm, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t from, int16_t to);
int32_t choose(int16_t first, int16_t last, const mem::Const<MenuEntry>& left, const mem::Const<MenuEntry>& right);
// fronts.cpp
void free_tubes();
void reset_fronts();
// record.cpp
void load_campaign();
void new_campaign();
void save_campaign(int16_t situation);
// groups.cpp (more)
void init_map_units();
void draw_units();
// frame.cpp (more)
int16_t sheltered();
int16_t hunt_check(int16_t g);
void pursue(int16_t unit);
void unit7_move();
// campaign.cpp (more 2)
void enemy_step(int16_t g);
int16_t group_wait(int16_t g);
int16_t group_state(int16_t g);
void unit_anim(int16_t n);
void map_frame();
int16_t sub_meets_unit();
// turn.cpp
void turn();
void map_screen();
void campaign_end();
int16_t enough_units();
// campaign.cpp (more)
int16_t pick_mission();
void choose_missions();
int16_t single_mission();
int16_t pick_second_mission();  // clock.cpp
// groups.cpp
int16_t form_group(const int8_t* types, int16_t g);  // types: up to kGroupMembers, 0 ends
// clock.cpp
void set_menu(int16_t index, const mem::Const<MenuEntry>& list);
// pages.cpp
int16_t picture_for(const char* name);
void ship_picture(const char* text, int16_t bitmap);
void conroom();
void base_report();
void briefing_page();
void loadout_screen();
void show_message(int16_t n);
void career_page();
void outcome_page(int16_t n);
void single_mission_end();
void end_phase();
// turn.cpp (more)
int16_t turn_check();
void mission_setup();
// record.cpp (more)
void reset_route_units();
void restore_positions();
// campaign.cpp (more 3)
void tally_battle();
void remove_losses();
void count_class(int16_t v);
void after_tally();
void promotion();
void ship_picture(const char* text, int16_t bitmap);
void count_terrain();
void draw_icons();
void print_header();
void ship_status_page();
int16_t string_width(const char* str);
void run_minutes();
void map_picture();
void print_status_word();
void day_message();
void campaign_page();
void print_hhmm(const Date& t);
void print_date(const Date& t);
void mission_names();
void message_page(int16_t n);
void enter_map_screen();
int16_t show_text_page(const char* text);
// loadout.cpp
void port_screen();
void mission_summary();
void loading_panel();
void status_line();
void tube_status_rows(int16_t inoperative);
// units.cpp
void text_screen();
// fronts.cpp
void init_fronts(int16_t settle);
// text.cpp
void fill_style(const screen::TextStyle& style);
void print_text(const char* text);
void text_object(screen::TextStyle& style, const char* str);
void cell_char(int16_t ch);
void two_digits(int16_t v);
void clear_text_page();
void set_column(int16_t n);
void set_row(int16_t n);
void new_line();
void print_name(int16_t code);
void number_cells(int16_t v, int16_t width, int16_t fill);
void clear_row_rest();
void clear_cells(int16_t n);
void clear_line_rest();
void text_char(int16_t ch);
const char* skip_strings(const char* p, int16_t n);  // past n NUL-ended strings
// frame.cpp
int16_t sub_at_base();
int16_t sub_near_target();
void enemy_tick3();
void enemy_tick5();
void detect(int16_t unit);
int16_t in_range(int16_t unit);

}  // namespace game::red
