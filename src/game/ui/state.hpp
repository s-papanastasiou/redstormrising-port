// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The state of the screen, input and menu code linked into the modules,
// kept in C++: one copy per module, as each module had its own in its
// small-data area. It moves here block by block; screen/vars.hpp and
// input/vars.hpp keep what is still in the emulated memory.
//
// Each field was a variable at an a4 offset of each module that has it
// (kFields; cleared at start or taken from the loaded image, see
// mem/module_state.hpp).
#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>

#include "game/mem/constants.hpp"
#include "game/mem/module_state.hpp"
#include "game/mem/types.hpp"
#include "game/screen/bitmap.hpp"
#include "game/screen/styles.hpp"

namespace game::screen {
struct TextStyle;
}  // namespace game::screen
namespace game::red {
struct MenuEntry;
}  // namespace game::red

namespace game::ui {

using mem::be16;
using mem::be32;
using mem::sbe16;
using mem::sbe32;

struct Point {
  int16_t x, y;
};

struct State {
  // The display.
  int16_t drawing_to;       // bitmap number drawn into
  int16_t showing;          // bitmap number shown
  int16_t palette_changed;  // the palette was set since the last frame
  int16_t current_shape;    // the shape loaded in shape_bitmap (-1: none)
  // Input.
  int16_t select_button;    // 1 select down, 2 menu down, 0 up
  int16_t last_key;         // getkey's / key_down's result
  int16_t quit_key;         // Alt-Q was pressed (ends waits and a battle)
  Point mouse_pos;          // the window's mouse position (each frame)
  // The menus (RedStart, Red).
  uint32_t menu_flags;      // input::kMenu* bits
  int16_t menu_key_code;    // the key read this frame
  int16_t menu_dirs;        // direction and fire bits
  Point menu_cursor;
  // RedStart's own copies of variables Red keeps elsewhere (its
  // initialised data; input/vars.hpp's PerModuleMixed point here).
  int16_t menu_bounds[4];   // left (also menu_request), top, right, bottom
  uint16_t tick_sound_a, tick_sound_b;
  int16_t steer_ref_x, steer_ref_y;
  int16_t no_return_fire;
  uint16_t medals_held;
  int16_t mouse_steering;   // the mouse steers while a button is down
  int16_t space_requests;   // Return/Space request instead of firing
  int16_t no_help_request;
  int16_t menu_toggle;      // flipped by character 173
  Point last_mouse;
  int16_t steer_step, steer_dx, steer_dy;
  // The frame tick (RedStart, Red).
  int16_t double_buffer;    // frame_tick flips the bitmaps
  int16_t flip_countdown;
  int16_t flip_bitmap;      // drawn into; shown at the next flip
  int16_t frame_ticked;     // set at a flip
  int16_t flips;
  int16_t tick_frames;      // every tick
  uint32_t last_tick;       // the timer at the last wait
  // The display's handles and buffers, the files in memory (set by
  // RedStorm, given to the modules through the record), kept as they
  // were: big-endian, addresses into the memory image.
  uint32_t gfx_base;
  uint32_t intuition_base;
  uint32_t diskfont_base;
  screen::BitMapRef bitmaps[3];
  screen::RastPortRef rastport;
  screen::ScreenRef screen;
  uint32_t window;
  uint32_t fonts[4];
  uint32_t planes_buffer_copy;
  uint32_t keyboard_port;  // (the original: its reply port; 0)
  uint32_t keyboard_io;    // 1 while keyboard.device is open (the original: its request)
  uint8_t key_matrix[13];  // keyboard.device's matrix as last read: 12 key bytes, then the qualifiers
  uint32_t shape_memory;
  uint32_t file_buffer;
  mem::Ptr<be32> frame_timer;
  int16_t button_event;
  uint32_t preloaded_anim_shapes;
  uint32_t preloaded_scripts;
  uint32_t preloaded_shapes;
  uint32_t preloaded_pictures;
  uint32_t preloaded_osg;
  uint32_t pointer_image;
  uint16_t pointer_kept;  // Storm: the pointer is not hidden for status messages
  mem::Ptr<sbe32> anim_shape_offsets;
  uint16_t anim_shapes_preloaded;
  uint16_t scripts_preloaded;
  mem::Ptr<sbe32> shape_offsets;
  uint16_t shapes_preloaded;
  mem::Ptr<sbe32> picture_offsets;
  uint16_t pictures_preloaded;
  mem::Ptr<sbe32> osg_offsets;
  uint16_t osg_preloaded;
  uint32_t dos_base;
  // The pointer images (RedStorm's, given through the record) and Storm's
  // image list.
  uint32_t pointer_8x8, pointer_7x7, pointer_none;
  mem::ConstPtr<int8_t> images;
  // The menus and text pages of Red (RedStart shares some of the code).
  uint16_t menu_keys;
  int16_t menu_index;
  uint8_t menu_side;  // bit 7: the right column
  mem::ConstPtr<red::MenuEntry> menu_list;  // the menu being chosen from
  screen::StyleRef window_style;
  screen::StyleRef row_colours;
  int16_t menu_page_top;
  uint32_t row_hook;  // a routine the menus may call per row
  mem::ConstPtr<uint8_t> text_list;  // text_n's list (packed texts in the constant data)
  uint8_t packed_text[256];  // unpack_text's input (a length byte, then the codes)
  char unpacked_text[512];   // text_n_ptr's result
  char window_text[2];       // a one-character string
  char symbol_text[2];       // Red: a map symbol's character
  char pending_text[48];     // Red: characters not drawn yet
  uint16_t palette[32];          // to load at the next show_bitmap ($0RGB)
  uint16_t module_palette[32];   // RedStart, Red: the colours read back (read_palette)
  uint16_t row_list[12];         // the rows' y that can not be chosen, ended by 0
  int16_t row_chosen;
  Point window_cursor;       // where window_char writes
  int16_t input_xs[32];        // RedStart: input_text's x of each character
  uint16_t pending_length;   // Red: how many (was pending_end, a pointer into it; not a field below)
  int16_t page_row;
  int16_t page_col;
  uint16_t page_shown;  // a text page is up (show_text_page)
  uint16_t unread_page_flag;  // cleared with the mode changes (nothing reads it)
  uint16_t text_page_drawn;  // set when a text page was shown
  screen::StyleRef page_text_style;  // the current text style
};

// Each module's copy (module_index() order), and the running module's.
extern State states[5];
State& state();

// Where each field was (mem/module_state.hpp): its a4 offset in storm,
// redstorm, title, redstart, red (X: the module has none) and its size (a
// Point is two words).
using mem::module_state::FieldAt;
inline constexpr int32_t X = mem::module_state::kAbsent;
#define UI_AT(member, size, ...) MODULE_FIELD(State, member, size, __VA_ARGS__)
#define UI_BE(member, size, ...) MODULE_FIELD_BE(State, member, size, __VA_ARGS__)
#define UI_N(member, size, elem, ...) MODULE_FIELD_N(State, member, size, elem, __VA_ARGS__)
inline constexpr FieldAt kFields[] = {
    // The screen/input library (every module; Storm's offset plus the
    // module's shift of the library's block).
    UI_AT(drawing_to, 2, -11660, 832, -23540, -17000, 10160),
    UI_AT(showing, 2, -11658, 834, -23538, -16998, 10162),
    UI_AT(palette_changed, 2, -10992, 1500, -22872, -16332, 10828),
    UI_AT(current_shape, 2, -11448, 1044, -23328, -16788, 10372),
    UI_AT(select_button, 2, -11414, 1078, -23294, -16754, 10406),
    UI_AT(last_key, 2, -11412, 1080, -23292, -16752, 10408),
    UI_AT(quit_key, 2, -32768, X, -12146, 16222, 0),
    // The window's mouse position, copied by the vertical-blank server
    // (the library's data block, 9740 in Storm).
    UI_AT(mouse_pos.x, 2, 9740, 10364, 4628, 29392, 17064),
    UI_AT(mouse_pos.y, 2, 9742, 10366, 4630, 29394, 17066),
    // The menu code (RedStart, Red).
    UI_AT(menu_flags, 4, X, X, X, 16312, 15990),
    UI_AT(menu_key_code, 2, X, X, X, 16310, 15988),
    UI_AT(menu_dirs, 2, X, X, X, 16316, 15994),
    UI_AT(menu_cursor.x, 2, X, X, X, 16306, 15984),
    UI_AT(menu_cursor.y, 2, X, X, X, 16308, 15986),
    UI_AT(menu_bounds[0], 2, X, X, X, 2, X),
    UI_AT(menu_bounds[1], 2, X, X, X, 4, X),
    UI_AT(menu_bounds[2], 2, X, X, X, 6, X),
    UI_AT(menu_bounds[3], 2, X, X, X, 8, X),
    UI_AT(tick_sound_a, 2, X, X, X, 46, X),
    UI_AT(tick_sound_b, 2, X, X, X, 48, X),
    UI_AT(steer_ref_x, 2, X, X, X, 168, X),
    UI_AT(steer_ref_y, 2, X, X, X, 170, X),
    UI_AT(no_return_fire, 2, X, X, X, -30866, X),
    UI_AT(medals_held, 2, X, X, X, 28, X),
    UI_AT(mouse_steering, 2, X, X, X, 16326, 15534),
    UI_AT(space_requests, 2, X, X, X, 16318, 15508),
    UI_AT(no_help_request, 2, X, X, X, 16328, 15532),
    UI_AT(menu_toggle, 2, X, X, X, -30872, 14364),
    UI_AT(last_mouse.x, 2, X, X, X, 30648, 17140),
    UI_AT(last_mouse.y, 2, X, X, X, 30650, 17142),
    UI_AT(steer_step, 2, X, X, X, 30652, 17144),
    UI_AT(steer_dx, 2, X, X, X, 30654, 17146),
    UI_AT(steer_dy, 2, X, X, X, 30656, 17148),
    UI_AT(double_buffer, 2, X, X, X, 16324, 15538),
    UI_AT(flip_countdown, 2, X, X, X, 14000, 15552),
    UI_AT(flip_bitmap, 2, X, X, X, 16304, 15982),
    UI_AT(frame_ticked, 2, X, X, X, 16320, 15542),
    UI_AT(flips, 2, X, X, X, 16332, 15550),
    UI_AT(tick_frames, 2, X, X, X, 16322, 15548),
    UI_AT(last_tick, 4, X, X, X, -15304, 11988),
    // The library's handles, buffers and files in memory (Storm's offsets
    // plus the module's shift of the library's two blocks).
    UI_N(gfx_base, 4, 4, 9424, 10048, 4312, 29076, 16748),
    UI_N(intuition_base, 4, 4, 9428, 10052, 4316, 29080, 16752),
    UI_N(diskfont_base, 4, 4, 9432, 10056, 4320, 29084, 16756),
    UI_BE(bitmaps[0], 4, 9436, 10060, 4324, 29088, 16760),
    UI_BE(bitmaps[1], 4, 9440, 10064, 4328, 29092, 16764),
    UI_BE(bitmaps[2], 4, 9444, 10068, 4332, 29096, 16768),
    UI_BE(rastport, 4, 9448, 10072, 4336, 29100, 16772),
    UI_BE(screen, 4, 9452, 10076, 4340, 29104, 16776),
    UI_N(window, 4, 4, 9456, 10080, 4344, 29108, 16780),
    UI_N(fonts[0], 4, 4, 9460, 10084, 4348, 29112, 16784),
    UI_N(fonts[1], 4, 4, 9464, 10088, 4352, 29116, 16788),
    UI_N(fonts[2], 4, 4, 9468, 10092, 4356, 29120, 16792),
    UI_N(fonts[3], 4, 4, 9472, 10096, 4360, 29124, 16796),
    UI_N(planes_buffer_copy, 4, 4, 9492, 10116, 4380, 29144, 16816),
    UI_N(keyboard_port, 4, 4, 9496, 10120, 4384, 29148, 16820),
    UI_N(keyboard_io, 4, 4, 9500, 10124, 4388, 29152, 16824),
    UI_BE(key_matrix, 13, 9504, 10128, 4392, 29156, 16828),
    UI_N(shape_memory, 4, 4, 9656, 10280, 4544, 29308, 16980),
    UI_N(file_buffer, 4, 4, 9700, 10324, 4588, 29352, 17024),
    UI_BE(frame_timer, 4, 9704, 10328, 4592, 29356, 17028),
    UI_N(button_event, 2, 2, 9708, 10332, 4596, 29360, 17032),
    UI_N(preloaded_anim_shapes, 4, 4, 9710, 10334, 4598, 29362, 17034),
    UI_N(preloaded_scripts, 4, 4, 9714, 10338, 4602, 29366, 17038),
    UI_N(preloaded_shapes, 4, 4, 9722, 10346, 4610, 29374, 17046),
    UI_N(preloaded_pictures, 4, 4, 9726, 10350, 4614, 29378, 17050),
    UI_N(preloaded_osg, 4, 4, 9730, 10354, 4618, 29382, 17054),
    UI_N(pointer_image, 4, 4, 9734, 10358, 4622, 29386, 17058),
    UI_N(pointer_kept, 2, 2, 9738, 10362, 4626, 29390, 17062),
    UI_BE(anim_shape_offsets, 4, -11446, 1046, -23326, -16786, 10374),
    UI_N(anim_shapes_preloaded, 2, 2, -11442, 1050, -23322, -16782, 10378),
    UI_N(scripts_preloaded, 2, 2, -11440, 1052, -23320, -16780, 10380),
    UI_BE(shape_offsets, 4, -11432, 1060, -23312, -16772, 10388),
    UI_N(shapes_preloaded, 2, 2, -11428, 1064, -23308, -16768, 10392),
    UI_BE(picture_offsets, 4, -11426, 1066, -23306, -16766, 10394),
    UI_N(pictures_preloaded, 2, 2, -11422, 1070, -23302, -16762, 10398),
    UI_BE(osg_offsets, 4, -11420, 1072, -23300, -16760, 10400),
    UI_N(osg_preloaded, 2, 2, -11416, 1076, -23296, -16756, 10404),
    UI_N(dos_base, 4, 4, 10196, 10688, 4984, 30932, 18568),
    // (RedStorm has none: its a4-3960/-3996/-3920 are the images themselves.)
    UI_N(pointer_8x8, 4, 4, -9308, X, -12158, 13988, 14370),
    UI_N(pointer_7x7, 4, 4, -9312, X, -12162, 13984, 14366),
    UI_N(pointer_none, 4, 4, -9304, X, -12154, 13992, 14374),
    UI_BE(images, 4, -9300, X, -12150, 13996, 14378),
    // The menus and text pages (RedStart, Red).
    UI_N(menu_keys, 2, 2, X, X, X, X, 15632),
    UI_N(menu_index, 2, 2, X, X, X, X, 15634),
    UI_BE(menu_side, 1, X, X, X, X, 15638),
    UI_BE(menu_list, 4, X, X, X, X, 15642),
    UI_BE(window_style, 4, X, X, X, -15624, 12036),
    UI_BE(row_colours, 4, X, X, X, -15564, 12096),
    UI_N(menu_page_top, 2, 2, X, X, X, -15560, 12100),
    UI_N(row_hook, 4, 4, X, X, X, -15596, 12064),
    UI_BE(text_list, 4, X, X, X, 29768, 17136),
    UI_BE(packed_text, 256, X, X, X, 29876, 17152),
    UI_BE(unpacked_text, 512, X, X, X, 30132, 17408),
    UI_BE(window_text, 2, X, X, X, -15558, 12102),
    UI_BE(symbol_text, 2, X, X, X, X, 132),
    UI_BE(pending_text, 48, X, X, X, X, 16132),
    UI_N(palette, 64, 2, 9744, 10368, 4632, 29396, 17068),
    UI_N(module_palette, 64, 2, X, X, X, 29810, 17958),
    UI_N(row_list, 24, 2, X, X, X, -15620, 12040),
    UI_N(row_chosen, 2, 2, X, X, X, 29760, 18308),
    UI_AT(window_cursor.x, 2, X, X, X, 29762, 18310),
    UI_AT(window_cursor.y, 2, X, X, X, 29764, 18312),
    UI_N(input_xs, 64, 2, X, X, X, 29696, X),
    UI_N(page_row, 2, 2, X, X, X, X, 15100),
    UI_N(page_col, 2, 2, X, X, X, X, 15102),
    UI_N(page_shown, 2, 2, X, X, X, 14004, 15536),
    UI_N(unread_page_flag, 2, 2, X, X, X, 14002, 15526),
    UI_N(text_page_drawn, 2, 2, X, X, X, 30644, 16128),
    UI_BE(page_text_style, 4, X, X, X, 16292, 14732),
};
#undef UI_AT
#undef UI_BE

}  // namespace game::ui
