// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Red (the campaign module): text pages and messages, and entering the
// strategic map's screen.
#include "game/common/strings.hpp"
#include "game/audio/calls.hpp"
#include "game/input/calls.hpp"
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/red/calls.hpp"
#include "game/red/texts_state.hpp"
#include "game/red/vars.hpp"
#include "game/screen/calls.hpp"
#include "game/ui/state.hpp"

namespace game::red {

namespace {
screen::TextStyle& style() { return *ui::state().page_text_style; }
uint16_t bit_of(int n) { n &= 63; return n < 16 ? uint16_t(1u << n) : 0; }

// The first two characters of a and b are the same (strncmp(a, b, 2) == 0).
bool same_start(const char* p, const char* q) {
  for (int i = 0; i < 2; ++i) {
    if (p[i] != q[i]) return false;
    if (!p[i]) return true;
  }
  return true;
}
}  // namespace

// $23c7c (RedStart $26400): a text page in the bitmap not shown, on its
// picture (texts starting "NL" have one of their own), then shown.
// Returns the bitmap.
int16_t show_text_page(const char* text) {
  map_controls() = 0;
  map_active() = 0;
  ui::state().page_shown = 1;
  ui::state().unread_page_flag = 0;
  screen::hide_pointer();
  ui::state().text_page_drawn = 1;
  screen::read_palette();
  const bool own_picture = same_start(text, picture_prefix.str());
  const int16_t b = screen::shown_bitmap() == 0 ? 1 : 0;
  screen::draw_to(b);
  screen::clear_screen();
  if (own_picture) {
    screen::load_picture(kPagePicture, b);
    ship_picture(text, b);
  } else {
    screen::load_picture(picture_for(text), b);
  }
  (ui::state().page_text_style)->bitmap = uint16_t(b);
  screen::show_bitmap(b);
  return b;
}
ORIGINAL_INSIDE(show_text_page, 0, 0, 0, 0x26400, 0x23c7c);

// $23d54: message n: a header line "!<c>.." (when there is one) shows its
// text page first; then the text in a window (message 1 at column 3 row
// 15, others at column 1 row 14).
void show_message(int16_t n) {
  char* text = const_cast<char*>(message(n));  // the unpacked buffer, edited in place
  char* body = text;
  if (body[0] == '!') {
    while (body[0] != '\n') ++body;
    body[0] = 0;
    ++body;
    rnd(8);  // compared with the header's character, for a call of an empty routine
    show_text_page(text + 3);
  }
  if (n == 1) {
    style().margin = 24;
    set_column(3);
    set_row(15);
  } else {
    style().margin = 8;
    set_column(1);
    set_row(14);
  }
  style().fg = 28;
  print_text(body);
  style().margin = 0;
  style().y = 0;
  style().height = 199;
  style().x = 0;
  style().width = kPageWidth;
}
ORIGINAL_IN("red", 0x23d54, show_message);

// $23e76: message n with the status line, for 30 seconds, then back to
// the map (unless a key is down).
void message_page(int16_t n) {
  if (quit_key_pressed()) return;
  if (!ui::state().osg_preloaded) screen::need_disk();
  show_message(n);
  if (n == kMessageWithName) {
    print_text(text_uss.str());
    print_text(texts().sub_name);
  }
  status_line();
  wait_seconds(30);
  text_screen();
}
ORIGINAL_IN("red", 0x23e76, message_page);

// $2d964: the map (picture 2) into bitmap 2 with the fronts drawn.
void map_picture() {
  screen::draw_to(kMapBitmap);
  symbol_style->bitmap = kMapBitmap;
  fill_style(*symbol_style);
  screen::load_picture(kMapPicture, kMapBitmap);
  init_fronts(1);
  map_controls() = 1;
}
ORIGINAL_IN("red", 0x2d964, map_picture);

// $2d8ce: to the map screen: the map into both shown bitmaps, bitmap 0
// shown and 1 drawn into, the map's animation on.
void enter_map_screen() {
  text_screen();
  if (quit_key_pressed()) return;
  map_picture();
  screen::blit_bitmaps(kMapBitmap, 0, 0, 0, 0, 0, 320, 200);
  screen::blit_bitmaps(kMapBitmap, 0, 0, 1, 0, 0, 320, 200);
  screen::show_bitmap(0);
  screen::draw_to(1);
  map_draw_bitmap() = 1;
  map_flip_countdown() = 0;
  map_redrawn() = 0;
  map_count() = 0;
  no_help_request() = 0;
  map_controls() = 1;
  map_active() = 1;
  map_double_buffer() = 1;
  input::flush_keys();
}
ORIGINAL_IN("red", 0x2d8ce, enter_map_screen);

// $233b6: the day's message (by the war's situation), if any.
void day_message() {
  const uint8_t n = (war.situation ? day_messages_b : day_messages_a)[phase.war_phase];
  if (n) message_page(n);
}
ORIGINAL_IN("red", 0x233b6, day_message);

// $22f38: the day's briefing message, if any.
void briefing_message() {
  const int16_t day = phase.war_phase;
  int16_t n;
  if (day >= 3 && day <= 8) {
    n = war.situation ? (loadout.unit_count >= 2 ? kBriefingMany : kBriefingFew) : 0;
  } else if (!war.situation) {
    n = tally.sunk >= tally.group_size ? kBriefingDone : briefings_first[day];
  } else if (phase.last_order_phase == 4) {
    const int16_t dest = unit_records[0].dest;
    n = (dest == 51 || dest == 61) ? briefings_dest_a[day] : briefings_dest_b[day];
  } else {
    n = briefings_second[day];
  }
  if (n) message_page(n);
}
ORIGINAL_IN("red", 0x22f38, briefing_message);

// $2cc88: the campaign page: "TOP SECRET", the date and time, the
// commander and the submarine, the day and the mission's names, then a
// wait for a key.
void campaign_page() {
  if (!turn_flags.names_shown) mission_names();
  ui::state().page_text_style = page_style.addr();
  const int16_t b = screen::shown_bitmap() != 1 ? 1 : 0;
  screen::draw_to(b);
  style().bitmap = uint16_t(b);
  screen::clear_screen();
  print_text(page_heading.str());
  style().fg = 17;
  screen::fill_style_fg(*page_style, kBannerX, 0, kBannerRight, kBannerBottom);
  style().fg = 0;
  style().bg = 17;
  screen::draw_text(*page_style, kBannerTextX, kBannerTextY, text_top_secret.str());
  style().fg = 17;
  style().bg = 0;
  set_column(8);
  set_row(0);
  style().fg = 7;
  print_hhmm(phase.time);
  print_text(text_colon2.str());
  print_date(phase.time);
  set_column(4);
  set_row(2);
  style().fg = 1;
  string_copy(number_text(), text_cmdr.str());
  string_append(number_text(), texts().commander_name);
  string_append(number_text(), text_usn.str());
  print_text(number_text());
  set_column(4);
  set_row(3);
  string_copy(number_text(), text_uss2.str());
  string_append(number_text(), texts().sub_name);
  print_text(number_text());
  screen::line_colour(0, 32, kPageWidth, 32, 3);
  set_column(27);
  set_row(6);
  style().fg = 24;
  text_char('#');
  number_cells(int16_t(phase.war_phase + 1), phase.war_phase > 0 ? 2 : 1, '0');
  text_char('-');
  for (int k = 0; k < 3; ++k) {
    if (k > 0) text_char('.');
    number_cells(int16_t(phase.mission_names[k]), 2, '0');
  }
  style().margin = 24;
  style().fg = 28;
  set_column(4);
  set_row(7);
  print_text(texts().mission_line);
  style().margin = 0;
  print_text(text_press_trigger.str());
  screen::set_palette(map_palette.array(screen::kPaletteWords));
  screen::show_bitmap(b);
  input::wait_key();
}
ORIGINAL_IN("red", 0x2cc88, campaign_page);
ORIGINAL_IN("red", 0x21ae4, campaign_page);  // a jump to it

// $2d432: the control room page: a sonar contact (with its bearing),
// the launch window, or no contacts; the menu, navigation, the time and
// date.
void briefing_page() {
  screen::set_palette(map_palette.array(screen::kPaletteWords));
  style().bitmap = uint16_t(show_text_page(text_conroom.str()));
  print_text(conroom_start.str());
  const int16_t state = mission.request;
  if (state != kRequestContact) print_text(conroom_no_contact.str());
  print_text(state == kRequestContact  ? conroom_battle_stations.str()
             : state == kRequestLaunch ? conroom_launch_window.str()
                                       : conroom_continue.str());
  print_text(conroom_menu.str());
  if (state != kRequestLaunch) {
    if (state == kRequestContact) {
      string_copy(number_text(), conroom_contact.str());
      const int16_t at = int16_t(((war.heading_unit8 + 8) & 15) * 3);
      for (int i = 0; i < 3; ++i) number_text()[kBearingAt + i] = bearing_digits[at + i];
      print_text(number_text());
    }
    print_status_word();
  }
  style().margin = 0;
  style().fg = 7;
  set_column(0);
  set_row(11);
  print_hhmm(clock);
  set_column(33);
  style().cursor_x -= 2;
  print_date(clock);
  screen::line_colour(0, 96, kPageWidth, 96, 3);
  screen::line_colour(0, 160, kPageWidth, 160, 3);
}
ORIGINAL_IN("red", 0x2d432, briefing_page);

// $2e1fa: count the colours of the 5x5 map pixels around the submarine.
void count_terrain() {
  war.terrain.other = 0;
  war.terrain.colour_26 = 0;
  war.terrain.fronts = 0;
  war.terrain.colour_1 = 0;
  map_picture();
  const Position& sub = map_units[kPlayerSub].pos;
  const int16_t y0 = sub.y >= 2 ? int16_t(sub.y - 2) : 0;
  int16_t x0 = sub.x >= 2 ? int16_t(sub.x - 2) : 0;
  if (x0 >= 310) x0 = 310;
  const int16_t previous = screen::drawing_bitmap();
  screen::draw_to(kMapBitmap);
  for (int16_t y = y0; y < y0 + 5; ++y)
    for (int16_t x = x0; x < x0 + 5; ++x) {
      const int16_t colour = map_pixel(x, y);
      int16_t& n = colour == 1 ? war.terrain.colour_1 : (colour == 2 || colour == 9) ? war.terrain.fronts
                 : colour == 26 ? war.terrain.colour_26 : war.terrain.other;
      n += 1;
    }
  screen::draw_to(previous);
}
ORIGINAL_IN("red", 0x2e1fa, count_terrain);

// $2d124: the ship status page: separator lines, the submarine's name,
// the time and date, the tube status.
void ship_status_page() {
  print_text(ship_status_text.str());
  print_text(text_tubes_at.str());
  screen::line_colour(253, 29, kPageWidth, 29, 3);
  screen::line_colour(200, 85, kPageWidth, 85, 3);
  screen::line_colour(124, 149, kPageWidth, 149, 3);
  if (loadout.flags & kShortList) {
    print_text(ship_status_short.str());
    screen::line_colour(178, 125, kPageWidth, 125, 3);
  }
  style().fg = 24;
  set_column(4);
  set_row(0);
  print_text(texts().sub_name);
  set_column(0);
  set_row(1);
  style().fg = 7;
  print_hhmm(clock);
  set_column(29);
  set_row(1);
  print_date(clock);
  tube_status_rows(1);
}
ORIGINAL_IN("red", 0x2d124, ship_status_page);

namespace {
// The text after a "!..." header line.
const char* after_header(const char* p) {
  if (p[0] == '!') {
    while (p[0] != '\n') ++p;
    ++p;
  }
  return p;
}
}  // namespace

// $231a0: the news: the war's (by the side ahead and the reserves) and
// the day's, one of their header pictures, both texts, the status line,
// for 30 seconds.
void news_report() {
  const int16_t a = scores.reserve_a, b = scores.reserve_b, day = phase.war_phase;
  if (quit_key_pressed()) return;
  if (!no_help_request()) clear_text_page();
  if (!ui::state().osg_preloaded) screen::need_disk();
  const bool second = war.situation != 0;
  const uint8_t day_news = (second ? news_day_b : news_day_a)[day];
  uint8_t front_news;
  if (tally.fronts_moving > tally.fronts_other) {
    front_news = news_ahead[a];
    if (front_news == 0) {
      const int16_t at = int16_t(2 * a);
      front_news = (second ? news_ahead_b : news_ahead_a)[at + rnd(2)];
    }
  } else {
    front_news = news_behind[b];
    if (front_news == 0) front_news = (second ? news_behind_b : news_behind_a)[b];
  }
  char* front = texts().news_front;
  char* day_text = news_day();
  string_copy(front, message(front_news));
  string_copy(day_text, message(day_news));
  char* shown = front[1] != '9' && rnd(2) != 0 ? front : day_text;
  char* end = shown;
  while (end[0] != '\n') ++end;
  end[0] = 0;
  show_text_page(shown + 3);
  end[0] = '\n';
  style().fg = 28;
  style().margin = 8;
  set_column(1);
  set_row(14);
  print_text(after_header(day_text));
  print_text(after_header(front));
  style().margin = 0;
  status_line();
  wait_seconds(30);
  text_screen();
}
ORIGINAL_IN("red", 0x231a0, news_report);

// $234f2: the campaign's end: its message (won, drawn, lost), then the
// map plays on for 8 hours.
void campaign_end() {
  turn_flags.no_pursuit = 1;
  turn_flags.no_picture_a = 1;
  const int16_t v = tally.score;
  message_page(v >= tally.group_size ? kEndWon : v >= 1 ? kEndDraw : kEndLost);
  turn_flags.hours_to_run = 8;
  turn_flags.minutes_to_run = 0;
  run_minutes();
  turn_flags.no_picture_a = 0;
  turn_flags.no_pursuit = 0;
}
ORIGINAL_IN("red", 0x234f2, campaign_end);

// $233ee: after a mission: its result message, the next rank bit, the
// new name (into the player's record), message 10, three days pass.
void promotion() {
  message_page((loadout.unit_bits & 0x10) ? 7 : (loadout.unit_bits & 0x20) ? 8 : 9);
  const int16_t r = player.rank;
  uint16_t& bits = player.rank_bits[r];
  bits |= bit_of(player.rank_bit);
  if (bits == 255) bits = rank_full[r];
  int16_t b = 0;
  while (bits & bit_of(b)) ++b;
  player.rank_bit = b;
  string_copy(texts().sub_name, message(int16_t(kRankMessages + (r << 3) + b)));
  PlayerRecord& rec = *career();
  rec.rank = uint16_t((player.rank << 3) | (player.rank_bit & 7));
  rec.rank_bits[r] = bits;
  for (int i = 0; i < kNameBytes; ++i) rec.sub_name[i] = texts().sub_name[i];
  message_page(kMessageWithName);
  advance_time(3, 0, 0);
  loadout.unit_bits = 0;
}
ORIGINAL_IN("red", 0x233ee, promotion);

// $22fda: the first medal the score has earned and not yet held: its
// page with the heading, the medal's name centred, the medal in its
// frame, for 30 seconds.
void award_medal() {
  if (quit_key_pressed()) return;
  screen::hide_pointer();
  if (war.situation) return;
  int16_t i = 0;
  for (; i < kMedals; ++i) {
    if (scores.factor < medal_scores[i]) continue;
    if (!(*medals_held & medal_masks[i])) break;
  }
  if (i >= kMedals) return;
  *medals_held |= medal_masks[i];
  const int16_t b = screen::shown_bitmap() != 1 ? 1 : 0;
  screen::draw_to(b);
  screen::set_palette(medal_palette.array(screen::kPaletteWords));
  screen::clear_screen();
  clear_text_page();
  set_column(1);
  set_row(2);
  style().margin = 8;
  style().fg = 28;
  print_text(message(kMedalHeading));
  style().fg = 13;
  string_copy(number_text(), message(medal_names[i]));
  const int16_t w = string_width(number_text());
  set_column(int16_t(int16_t(40 - w / 8) >> 1));
  set_row(22);
  print_text(number_text());
  screen::draw_masked(kMedalFrameShape, kMedalFrameX, kMedalFrameY, 0, 0);
  const int16_t x = int16_t(kMedalFrameX + (int16_t(kMedalAreaW - medal_widths[i]) >> 1));
  const int16_t y = int16_t(kMedalFrameY + (int16_t(kMedalAreaH - medal_heights[i]) >> 1));
  screen::draw_masked(medal_shapes[i], x, y, 0, 0);
  screen::show_bitmap(b);
  wait_seconds(30);
  style().margin = 0;
}
ORIGINAL_IN("red", 0x22fda, award_medal);

// $237aa: the career page: the outcome picture and text (or, at the
// campaign's end, the last screen with the final rank and badge), the
// tally of ships by class, the medals; at the end the record keeps the
// final score, medals and rank.
void career_page() {
  if (quit_key_pressed()) return;
  ui::state().page_text_style = page_style.addr();
  const int16_t b = screen::shown_bitmap() != 1 ? 1 : 0;
  screen::draw_to(b);
  style().bitmap = uint16_t(b);
  screen::clear_screen();
  const char* text;
  if (turn_flags.turn_over) {
    screen::blit(screen::bitmap_at(b == 0 ? 1 : 0), 0, 0, screen::bitmap_at(b), 0, 0, 320, 200);
    text = skip_strings(rank_names.str(), scores.outcome.final_rank);
  } else {
    const bool won = battles.won > battles.lost;
    screen::draw_masked(career_pictures[won ? 3 : 1], 0, 0, 0, 0);
    string_copy(texts().news_front, message(won ? kMessageWon : kMessageLost));
    text = texts().news_front;
    battles.lost = 0;
    battles.won = 0;
  }
  screen::draw_masked(kCareerFrameShape, kCareerFrameX, 0, 0, 0);
  style().fg = 1;
  style().bg = 31;
  style().margin = 0;
  style().y = 0;
  style().height = 95;
  style().x = 96;
  style().width = int16_t(kPageWidth - 96);
  set_column(14);
  set_row(1);
  const int16_t m = war.situation == kSituationSingle ? (mission.order < 8 ? kMessageCareerLate : kMessageCareerLate + 1)
                                               : kMessageCareer;
  print_text(message(m));
  style().margin = 16;
  set_column(14);
  set_row(3);
  style().fg = 3;
  print_text(message(kMessageTally));
  set_column(14);
  set_row(4);
  style().fg = 13;
  for (const uint16_t n : {scores.class_high, scores.class_top, scores.class_low, scores.class_mid}) {
    two_digits(int16_t(n));
    new_line();
  }
  draw_icons();
  if (turn_flags.turn_over) {
    print_text(text_final_rank.str());
    print_text(text);
    const int8_t badge = scores.outcome.badge;
    if (badge >= 0) screen::draw_masked(badge_shapes[badge], kBadgeX, kBadgeY, 0, 0);
  } else {
    style().margin = 8;
    set_column(13);
    set_row(9);
    style().fg = 1;
    style().bg = 31;
    print_text(text);
  }
  print_header();
  print_text(text_press_trigger.str());
  screen::set_palette(medal_palette.array(screen::kPaletteWords));
  screen::show_bitmap(b);
  input::wait_key();
  text_screen();
  screen::set_palette(map_palette.array(screen::kPaletteWords));
  if (turn_flags.turn_over) {
    PlayerRecord& rec = *career();
    rec.score = scores.factor;
    rec.medals = *medals_held;
    rec.final_rank = scores.outcome.record_final_rank;
    rec.ending_rank = scores.outcome.record_ending_rank;
  }
}
ORIGINAL_IN("red", 0x237aa, career_page);

namespace {
// The outcome record from row k of a table.
void set_outcome(const Const<uint8_t>& table, int16_t k) {
  uint8_t bytes[kOutcomeBytes];
  for (int i = 0; i < kOutcomeBytes; ++i) bytes[i] = table[k * kOutcomeBytes + i];
  image::read(bytes, scores.outcome);
}
}  // namespace

// $235e8: an ending's outcome page: message n (but its first line) beside
// the outcome's career picture, under the headline (a single mission's:
// orders before 8 or from 8 on; the medals with it), for 60 seconds;
// then the career page.
void outcome_page(int16_t n) {
  if (quit_key_pressed()) return;
  const int16_t b = screen::shown_bitmap() != 1 ? 1 : 0;
  screen::draw_to(b);
  style().bitmap = uint16_t(b);
  screen::clear_screen();
  screen::set_palette(medal_palette.array(screen::kPaletteWords));
  char* news = texts().news_front;
  string_copy(news, message(n));
  int body = 0;
  while (news[body++] != '\n') {}
  news[body - 1] = 0;
  screen::draw_masked(career_pictures[scores.outcome.picture], 0, 0, 0, 0);
  screen::draw_masked(kCareerFrameShape, kCareerFrameX, 0, 0, 0);
  style().margin = 0;
  style().y = 0;
  style().height = 95;
  style().x = 96;
  style().width = int16_t(kPageWidth - 96);
  style().fg = 1;
  style().bg = 31;
  set_column(14);
  set_row(1);
  const int16_t headline = war.situation == kSituationSingle
                               ? int16_t(kMessageMissionOver + (mission.order < kLateOrders ? 0 : 1))
                               : kMessageOutcome;
  print_text(message(headline));
  style().margin = 8;
  set_column(13);
  set_row(3);
  style().fg = 3;
  style().bg = 31;
  print_text(news + body);
  if (headline > kMessageOutcome) draw_icons();
  print_header();
  screen::show_bitmap(b);
  input::wait_seconds(kOutcomeSeconds);
  career_page();
}
ORIGINAL_IN("red", 0x235e8, outcome_page);

// $21a3e: the end of a single mission: its outcome (orders from 8 on:
// whether anything scored; else the war flag, nothing scored, or the
// score against the enemy group's size, by the spread) and medals, then
// the outcome page.
void single_mission_end() {
  turn_flags.turn_over = 1;
  score_factor();
  const int16_t score = tally.score;
  int16_t k;
  if (mission.order >= kLateOrders) k = score ? 1 : 0;
  else if (turn_flags.war_flag) k = 2;
  else if (!score) k = 3;
  else if (score < tally.group_size) k = player.spread < 2 ? 4 : 5;
  else k = player.spread < 2 ? 6 : 7;
  set_outcome(single_outcomes, k);
  *medals_held = single_medals[k];
  outcome_page(scores.outcome.first_page);
}
ORIGINAL_IN("red", 0x21a3e, single_mission_end);

// $2353c: the end of the war: its outcome (before or after 13 missions;
// the last phase or not; the situation; the spread), two message pages
// and the outcome page.
void end_phase() {
  map_controls() = 0;
  turn_flags.turn_over = 1;
  mission.request = 2;
  const bool last_phase = phase.war_phase == kLastWarPhase, situation = war.situation != 0;
  int16_t k;
  if (scores.missions < kLongWar) {
    if (last_phase) k = situation ? 0 : 1;
    else k = situation ? 2 : (player.spread == kSpreadHigh ? 4 : 3);
  } else {
    if (last_phase) k = situation ? 5 : 6;
    else k = situation ? 7 : 8;
  }
  set_outcome(war_outcomes, k);
  const int16_t page = scores.outcome.first_page;
  message_page(page);
  message_page(int16_t(page + 1));
  outcome_page(int16_t(page + 2));
}
ORIGINAL_IN("red", 0x2353c, end_phase);

// $28274 (RedStart $26d98): the picture of the ship named at the start of `text` (from
// the OSG file, or its copy in memory) drawn at (166, 0) into `bitmap`.
void ship_picture(const char* text, int16_t bitmap) {
  const int16_t n = common::find_name(text);
  if (n == kNoShip) screen::fatal_error(osg_overrun_text.str());
  const Ptr<sbe32> offsets = ui::state().osg_offsets;
  const int32_t offset = offsets[n], length = offsets[n + 1] - offsets[n];
  const uint32_t data = ui::state().osg_preloaded ? ui::state().preloaded_osg + uint32_t(offset)
                                       : screen::read_file_part(osg_file.str(), osg_missing_text.str(), offset, length);
  screen::build_shape(data, length);
  screen::draw_shape(screen::bitmap(screen::shape_bitmap.addr()), screen::bitmap_at(bitmap), kShipX, 0);
  ui::state().current_shape = -1;
}
ORIGINAL_INSIDE(ship_picture, 0, 0, 0, 0x26d98, 0x28274);

// $23ef2 (RedStart $26676): the picture named by a text page's header (the last match of
// the twelve names); an unknown one is fatal.
int16_t picture_for(const char* name) {
  int16_t found = -1;
  for (int16_t i = 0; i < kPictures; ++i)
    if (std::strcmp(const_text(picture_names[i]), name) == 0) found = i;
  if (found == -1) screen::fatal_error(name);
  return found;
}
ORIGINAL_INSIDE(picture_for, 0, 0, 0, 0x26676, 0x23ef2);

// $2a9f8: the control room until the captain leaves it: the loadout, the
// campaign page, or no battle (situation 40); back on the map for 4.
void conroom() {
  for (bool running = true; !quit_key_pressed() && running;) {
    briefing_page();
    set_menu(mission.request != kRequestLaunch ? 1 : 0, conroom_menu_list);
    switch (uint8_t(choose(0, 3, conroom_menu_list, conroom_menu_list))) {
      case kConroomLeave:
        running = false;
        break;
      case kConroomLoadout:
        loadout_screen();
        break;
      case kConroomCampaign:
        screen::read_palette();
        campaign_page();
        screen::default_palette();
        break;
      case kConroomNoBattle:
        war.situation = kSituationNoBattle;
        mission.request_copy = mission.request;
        mission.request = 1;
        running = false;
        break;
      default:
        break;
    }
  }
  if (!quit_key_pressed() && mission.request == 4) {
    map_controls() = 0;
    enter_map_screen();
  }
}
ORIGINAL_IN("red", 0x2a9f8, conroom);

// $2abec: the report at the base: message 1, a sound, the career page,
// then the port.
void base_report() {
  show_message(1);
  audio::play_effect(kEffectBaseReport);
  wait_seconds(30);
  text_screen();
  career_page();
  screen::set_palette(map_palette.array(screen::kPaletteWords));
  port_screen();
}
ORIGINAL_IN("red", 0x2abec, base_report);
ORIGINAL_IN("red", 0x20c60, base_report);  // a jump to it

// $2f184: dos.library's Delay (`ticks` fiftieths of a second).
void delay(int32_t ticks) { os::delay(ticks); }
ORIGINAL(delay, 0, 0, 0, 0, 0x2f184);

}  // namespace game::red
