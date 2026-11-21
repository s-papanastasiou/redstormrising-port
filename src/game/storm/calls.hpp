// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Storm: the module's routines, by the file that defines them.
#pragma once

#include <cstdint>

#include "game/mem/types.hpp"

#include "game/common/math.hpp"

#include "game/storm/state.hpp"

namespace game::storm {

int16_t battle_main();  // battle.cpp
// pointer.cpp
void open_input();
void close_input();
void move_pointer(int16_t x, int16_t y, int16_t relative);
void identify_update();

// text.cpp
void text(const char* str, int16_t x, int16_t y, int16_t colour);
void text_shadow(const char* str, int16_t x, int16_t y, int16_t colour);
void status_message(const char* str, int16_t colour, int16_t remember);
void text_at_cell(const char* str, int16_t col, int16_t row, int16_t colour);
void status_bearing(const char* name, int16_t angle, int16_t colour);
void text_jam2(const char* str, int16_t x, int16_t y, int16_t fg, int16_t bg);
void tactical_background();
void torpedo_labels();
void draw_letter_list();
void own_ship_panel();
void left_panel();
void right_panel();
void sea_noise();
void torpedo_panel();
void helm_panel();
void shade_column(int16_t phase, int16_t type, int16_t x, int16_t y);
void helm_labels();
void sonar_library();
void map_view();
void aim_to_screen();
void show_event(int16_t what, int16_t unused, int16_t cls, int16_t final);
void load_aim(int16_t w);
void draw_display();
void set_display(int16_t display, int16_t page);
void entry_line();
void fresh_screen();
void random_sea();
void periscope_display();
void periscope_view();
void weapon_panel();
void display_setup();
void sonar_update();
void contact_label_panel();
void type_data_page(int16_t type);
void contact_panel();
void move_ship(int16_t n, int16_t turn);
void hear(int16_t listener, int16_t target);
void ship_tactics(int16_t n);
void enemy_weapon_step(int16_t i);
void object_seek(int16_t i);
void player_weapon_step(int16_t i);
void tactical_display();
void battle_init();
void general_quarters_report();
void after_action_report();
void save_battle_state();
void redraw();
void engine_update();
int16_t handle_keys();
void weapon_step();
void battle_step();
void replay_log();
int16_t process_key(int16_t key);
void tactical_analysis();
void weapon_page();
void order_key(int16_t key);
void enter_mode3();
void enter_mode5();
void enter_mode3b();
void enter_display1();
void enter_display5();
void change_scale(int16_t dir);
void change_speed(int16_t steps);
void change_rudder(int16_t dir);
void tubes_page(int16_t reloads);
void start_aiming(int16_t type);
void refresh_status();
void refresh_status2();
void replay_battle();
void flash_red();
void flash_black();
void rbu_attack();
void enemy_launch(int16_t n, int16_t heading, int16_t a, int16_t b);
int16_t replay_step();
int16_t in_map_area(int16_t x, int16_t y);
void mark(int16_t kind, int16_t x, int16_t y);
void text_on_bitmap2(const char* str, int16_t x, int16_t y, int16_t colour);
void sonar_analysis_display();
void contact_display();
void weapon_page();
void depth_chart();
void sonar_display();
void sonar_library_display();
void letters_display();
void gauge(int16_t value, int16_t row, int16_t colour);
void draw_char(int16_t ch, int16_t x, int16_t y, int16_t colour, int16_t bitmap);
void random_conditions();

// A ship type's sonar shades: the pens of its 32 lines, in order
// (panels.cpp).
struct ShipType;
class ShadeLevels {
public:
  ShadeLevels(const ShipType& t, int16_t phase);
  int16_t next_pen(int16_t i);

private:
  int16_t level_, step_, mask_, phase_;
};

void clear_status();
void status_if_flag();

// ships.cpp
void keep_highest(int16_t level, int16_t value);
int16_t area_level(int16_t x, int16_t y);  // the sea bed's level around a map point (-3..4)
void roll_highest();
void remove_object(int16_t i);
void track_ship(int16_t i);
void approach(int16_t i);
void random_damage();
void log_all();
int16_t launch(int16_t kind, int16_t ship);
void hit_ship(int16_t n, int16_t kind);
void object_step(int16_t i);
void sea_shades();
void order_course(int16_t deg);
void order_depth(int16_t v);
void unload_tube(int16_t n);
void wait_frames(int16_t n);
void fire_weapon();
void detect(int16_t observer, int16_t target);
void next_contact();
int16_t below_limit(int16_t depth);
int16_t aim_cursor();
void attack_check();
void text_on_bitmap1(const char* str, int16_t x, int16_t y, int16_t fg, int16_t bg);
int16_t attack_score(const Body& attacker, const Body& target, int16_t kind, int16_t heading, int16_t range, int16_t bonus);
void draw_ticks();

// battle.cpp
void start_message(int16_t i);
void load_tube(int16_t weapon);
void select_next_contact();
int16_t depth_at(int16_t x, int16_t y);
void report_events();
int16_t log_event(int16_t n);
int16_t log_ship_event(int16_t n);
void show_mode(int16_t mode, int16_t colour);

// ships.cpp
void steer_torpedo();
using common::course_to;  // $33174: bearing and range into course_bearing/range (common/math.cpp)

}  // namespace game::storm
