// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// intuition.library high-level emulation: custom screens, a backdrop window
// per screen, IDCMP RAWKEY/MOUSEBUTTONS/MOUSEMOVE events, the mouse
// position and the sprite pointer. Screens and windows are host objects
// (intuition_objects.hpp) named by the addresses the
// original's structures had; the shown screens become the display's bands
// (Hle::build_bands). There are no layers: window RastPorts draw straight
// into the screen bitmap (all of the game's windows are borderless
// backdrops).
#include "amiga/hle.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace amiga {

namespace {
// Where a Screen's RastPort and BitMap were (their handles).
constexpr uint32_t SC_RASTPORT = 84, SC_BITMAP = 184;
constexpr uint32_t IDCMP_MOUSEBUTTONS = 0x8, IDCMP_MOUSEMOVE = 0x10, IDCMP_RAWKEY = 0x400;
constexpr uint32_t WFLG_REPORTMOUSE = 0x200;
constexpr uint16_t SELECTDOWN = 0x68, MENUDOWN = 0x69, UPCODE = 0x80;
constexpr uint32_t MP_SIZE = 34;
}  // namespace


void Hle::intuition_init() {
  mouse_x_ = FB_W / 2;
  mouse_y_ = FB_H / 2;
}

namespace {
void moved(Machine& m, uint32_t addr, uint32_t size, bool is_moved) {
  if (game_hooks.object_moved) game_hooks.object_moved(addr, size, is_moved);
  (void)m;
}
}  // namespace

Screen& Hle::screen(uint32_t handle) {
  auto it = screen_objs_.find(handle);
  if (it == screen_objs_.end()) {
    m.log("screen %06x is not open\n", handle);
    std::abort();
  }
  return it->second;
}

Window& Hle::window(uint32_t handle) {
  auto it = window_objs_.find(handle);
  if (it == window_objs_.end()) {
    m.log("window %06x is not open\n", handle);
    std::abort();
  }
  return it->second;
}

// The screens shown (front screen first) as the display's bands: MrgCop +
// LoadView.
void Hle::rethink_display() {
  build_bands();
  m.display_dma_ = true;
}

uint32_t Hle::open_screen(const NewScreen& ns) {
  const int16_t width = ns.width > 0 ? ns.width : int16_t(FB_W);
  const int16_t height = ns.height > 0 ? ns.height : int16_t(FB_H);
  const int depth = std::clamp<int>(ns.depth, 1, 6);
  const uint16_t modes = ns.modes;
  const uint16_t type = ns.type;
  const uint32_t custom_bm = ns.custom_bitmap;
  const uint32_t s = m.alloc(kScreenBytes);
  if (!s) return 0;
  Screen& sc = screen_objs_[s];
  sc = Screen{};
  sc.handle = s;
  sc.left = ns.left;
  sc.top = ns.top;
  sc.width = width;
  sc.height = height;
  sc.flags = type;
  sc.title = ns.title;
  sc.detail_pen = ns.detail_pen;
  sc.block_pen = ns.block_pen;
  // BitMap: the caller's (CUSTOMBITMAP) or our own planes in chip memory.
  sc.bitmap = s + SC_BITMAP;
  const uint16_t bpr = uint16_t(((width + 15) >> 4) * 2);
  m.forget_bitmaps(sc.bitmap, kBitMapBytes);
  BitMap& b = m.bitmap(sc.bitmap);  // (the Screen's memory is cleared: a cleared BitMap)
  if ((type & 0x40) && custom_bm) {
    b = m.bitmap(custom_bm);
  } else {
    b.bytes_per_row = bpr;
    b.rows = uint16_t(height);
    b.depth = uint8_t(depth);
    for (int p = 0; p < depth; ++p) b.planes[p] = m.arena_alloc(uint32_t(bpr) * uint32_t(height));  // chip memory
  }
  // ViewPort with RasInfo and a 32-entry ColorMap (their memory as before).
  ViewPort& vp = sc.viewport;
  vp.dwidth = width;
  vp.dheight = height;
  vp.dxoffset = sc.left;
  vp.dyoffset = sc.top;
  vp.modes = modes;
  vp.rasinfo_handle = m.alloc(12);
  vp.bitmap = sc.bitmap;
  vp.colormap_handle = m.alloc(8 + 64);
  static const uint16_t defaults[4] = {0x05A, 0xFFF, 0x002, 0xF80};
  for (int i = 0; i < 32; ++i) vp.colormap.table[i] = defaults[i & 3];
  vp.dsp = m.alloc(16);  // DspIns token (see MakeVPort)
  // Screen RastPort over the bitmap.
  sc.rastport = s + SC_RASTPORT;
  init_rastport(sc.rastport);
  m.rastport(sc.rastport).bitmap = sc.bitmap;
  moved(m, s, kScreenBytes, true);
  screens_.insert(screens_.begin(), s);
  active_screen_ = s;
  rethink_display();
  m.log("OpenScreen %dx%d depth %d modes %04x = %06x\n", width, height, depth, modes, s);
  return s;
}

void Hle::close_screen(uint32_t s) {
  screens_.erase(std::remove(screens_.begin(), screens_.end(), s), screens_.end());
  if (active_screen_ == s) active_screen_ = screens_.empty() ? 0 : screens_.front();
  rethink_display();
}

void Hle::init_rastport(uint32_t rp) {
  m.forget_bitmaps(rp, kRastPortBytes);  // (and a RastPort there before)
  for (uint32_t i = 0; i < kRastPortBytes; ++i) m.w8(rp + i, 0);
  RastPort& r = m.rastport(rp);
  r.mask = 0xFF;
  r.fg_pen = 1;
  r.aol_pen = 1;
  r.draw_mode = 1;  // JAM2
  r.line_ptrn = 0xFFFF;
}

uint32_t Hle::open_window(const NewWindow& nw) {
  uint32_t s = nw.screen;  // CUSTOMSCREEN
  if (!s && !screens_.empty()) s = screens_.front();
  if (!s) { m.log("OpenWindow: no screen\n"); return 0; }
  const uint32_t w = m.alloc(kWindowBytes);
  if (!w) return 0;
  Window& wd = window_objs_[w];
  wd = Window{};
  wd.handle = w;
  wd.left = nw.left;
  wd.top = nw.top;
  wd.width = nw.width;
  wd.height = nw.height;
  wd.detail_pen = nw.detail_pen;
  wd.block_pen = nw.block_pen;
  wd.idcmp = nw.idcmp;
  wd.flags = nw.flags;
  wd.title = nw.title;
  wd.screen = s;
  // Own RastPort drawing straight into the screen bitmap (no layers).
  wd.rastport = m.alloc(kRastPortBytes);
  init_rastport(wd.rastport);
  m.rastport(wd.rastport).bitmap = screen(s).bitmap;
  if (wd.idcmp) {
    // UserPort signalling the opening task (its memory and signal as the
    // original's; the messages are host events, send_idcmp).
    m.alloc(MP_SIZE);
    int bit = -1;
    for (int i = 31; i >= 16; --i)
      if (!(cur().sig_alloc & (1u << i))) { bit = i; break; }
    if (bit >= 0) cur().sig_alloc |= 1u << bit;
    wd.user_port = true;
    wd.signal = bit < 0 ? 16 : bit;
  }
  moved(m, w, kWindowBytes, true);
  windows_.push_back(w);
  if (wd.flags & 0x1000) active_window_ = w;  // ACTIVATE
  update_mouse_fields();
  m.log("OpenWindow %dx%d IDCMP %08x flags %08x = %06x\n", wd.width, wd.height, wd.idcmp, wd.flags, w);
  return w;
}

void Hle::close_window(uint32_t w) {
  windows_.erase(std::remove(windows_.begin(), windows_.end(), w), windows_.end());
  if (active_window_ == w) active_window_ = windows_.empty() ? 0 : windows_.back();
  const Window& wd = window(w);
  if (wd.user_port) cur().sig_alloc &= ~(1u << wd.signal);
  if (pointer_window_ == w) pointer_window_ = 0;
}

// The pointer position, clamped to the front screen, in the windows
// (MouseX/Y relative to their top-left corner).
void Hle::update_mouse_fields() {
  for (uint32_t w : windows_) {
    Window& wd = window(w);
    wd.mouse_x = int16_t(mouse_x_ - wd.left);
    wd.mouse_y = int16_t(mouse_y_ - wd.top);
  }
}

// An input event for the active window, if it asked for this class. The
// game reads them with next_input (they were IntuiMessages on the window's
// UserPort; the game took every field it used from them at once).
void Hle::send_idcmp(uint32_t cls, uint16_t code) {
  if (!active_window_) return;
  const Window& wd = window(active_window_);
  if (!(wd.idcmp & cls) || !wd.user_port) return;
  if (input_events_.size() >= 64) return;  // the game is not reading
  input_events_.push_back({cls, code, qualifier_});
}

bool Hle::next_input(InputEvent& event) {
  if (input_events_.empty()) return false;
  event = input_events_.front();
  input_events_.pop_front();
  return true;
}

void Hle::mouse_move(int dx, int dy) {
  int sw = FB_W, sh = FB_H;
  if (!screens_.empty()) {
    sw = screen(screens_.front()).width;
    sh = screen(screens_.front()).height;
  }
  mouse_x_ = std::clamp(mouse_x_ + dx, 0, sw - 1);
  mouse_y_ = std::clamp(mouse_y_ + dy, 0, sh - 1);
  update_mouse_fields();
  if (active_window_ && (window(active_window_).flags & WFLG_REPORTMOUSE)) send_idcmp(IDCMP_MOUSEMOVE, 0);
}

void Hle::pointer_event(uint8_t cls, uint16_t qualifier, int16_t x, int16_t y) {
  constexpr uint8_t IECLASS_RAWMOUSE = 2, IECLASS_POINTERPOS = 4;
  constexpr uint16_t IEQUALIFIER_RELATIVEMOUSE = 0x8000;
  if (cls == IECLASS_RAWMOUSE && (qualifier & IEQUALIFIER_RELATIVEMOUSE))
    mouse_move(x / 2, y / 2);
  else if (cls == IECLASS_POINTERPOS)
    mouse_move(x / 2 - mouse_x_, y / 2 - mouse_y_);
}

void Hle::mouse_button(int button, bool down) {
  const uint16_t q = button == 0 ? 0x4000 : 0x2000;  // IEQUALIFIER_LEFTBUTTON/RBUTTON
  if (down) qualifier_ |= q; else qualifier_ &= uint16_t(~q);
  send_idcmp(IDCMP_MOUSEBUTTONS,
             uint16_t((button == 0 ? SELECTDOWN : MENUDOWN) | (down ? 0 : UPCODE)));
}

void Hle::rawkey_idcmp(uint8_t rawkey, bool down) {
  send_idcmp(IDCMP_RAWKEY, uint16_t(rawkey | (down ? 0 : UPCODE)));
}

// Sprite 0 pointer image (SetPointer), drawn over the frame like the
// hardware sprite: colours 17-19, hot spot at (XOffset, YOffset).
void Hle::draw_pointer(uint32_t* fb) {
  const uint32_t w = pointer_window_;
  if (!w || active_window_ != w) return;
  const Window& wd = window(w);
  const int h = wd.ptr_height;
  if (!wd.pointer || !h) return;
  const int x0 = mouse_x_ + wd.x_offset;
  const int y0 = mouse_y_ + wd.y_offset;
  auto rgb = [&](int reg) {
    const uint16_t c = m.color_reg(reg);
    return uint32_t(((c >> 8) & 15) * 17) << 16 | uint32_t(((c >> 4) & 15) * 17) << 8 |
           uint32_t((c & 15) * 17);
  };
  for (int y = 0; y < h; ++y) {
    const size_t row = 2 + size_t(y) * 2;
    const uint16_t p0 = row < wd.pointer_words.size() ? wd.pointer_words[row] : 0;
    const uint16_t p1 = row + 1 < wd.pointer_words.size() ? wd.pointer_words[row + 1] : 0;
    const int fy = y0 + y;
    if (fy < 0 || fy >= FB_H) continue;
    for (int x = 0; x < 16; ++x) {
      const int c = ((p0 >> (15 - x)) & 1) | (((p1 >> (15 - x)) & 1) << 1);
      const int fx = x0 + x;
      if (!c || fx < 0 || fx >= FB_W) continue;
      fb[size_t(fy) * FB_W + size_t(fx)] = rgb(16 + c);
    }
  }
}

void Hle::screen_to_front(uint32_t s) {
  screens_.erase(std::remove(screens_.begin(), screens_.end(), s), screens_.end());
  screens_.insert(screens_.begin(), s);
  rethink_display();
}

void Hle::set_pointer(uint32_t w, uint32_t image, const std::vector<uint16_t>& words, int height, int width,
                      int x_offset, int y_offset) {
  Window& wd = window(w);
  wd.pointer = image;
  wd.pointer_words = words;
  wd.ptr_height = uint8_t(height);
  wd.ptr_width = uint8_t(width);
  wd.x_offset = int8_t(x_offset);
  wd.y_offset = int8_t(y_offset);
  pointer_window_ = w;
}

void Hle::activate_window(uint32_t w) { active_window_ = w; }

int32_t Hle::display_alert(uint32_t number, const char* text) {
  m.log("DisplayAlert(%08x): %s \n", number, text);
  return 0;
}

}  // namespace amiga
