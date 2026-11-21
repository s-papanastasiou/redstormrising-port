// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// The custom chips as far as the game uses them: the display (bitplanes
// and colours per screen band, rendered at the frame's end), the blitter
// (a blit runs at once), Paula's four audio channels, the mouse.
// Forked from fa18-interceptor-port src/amiga/custom.cpp (c586ff7).
#include "amiga/machine.hpp"
#include "amiga/blitter_fill.hpp"
#include "amiga/blitter_minterm.hpp"
#include "amiga/hle.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <set>

namespace amiga {

namespace {

inline uint32_t rgb12(uint16_t c) {
  uint32_t r = (c >> 8) & 15, g = (c >> 4) & 15, b = c & 15;
  return (r * 17) << 16 | (g * 17) << 8 | (b * 17);
}
}  // namespace

// ---- the bus outside RAM ------------------------------------------------------

// No custom chip or CIA register is reached any more: the game's
// blitter, ports and display go through typed calls. An access outside RAM
// reads 0 and is logged (the gate fails on the log line). RAM pages that
// hold a module's image come here too: resolved by bytes().
namespace {
void unmapped(Machine& m, uint32_t addr, bool write) {
  static std::set<uint32_t> seen;
  if (seen.insert(addr).second) m.log("[io] %s of unmapped %06x\n", write ? "write" : "read", addr);
}
}  // namespace

uint8_t Machine::read8(uint32_t addr) {
  addr &= 0xFFFFFF;
  if (addr < RAM_SIZE) return *bytes(addr, 1);
  unmapped(*this, addr, false);
  return 0;
}
uint16_t Machine::read16(uint32_t addr) {
  addr &= 0xFFFFFF;
  if (addr + 2 <= RAM_SIZE) return bytes16(addr);
  unmapped(*this, addr, false);
  return 0;
}
void Machine::write8(uint32_t addr, uint8_t v) {
  addr &= 0xFFFFFF;
  if (addr < RAM_SIZE) *bytes(addr, 1) = v;
  else unmapped(*this, addr, true);
}
void Machine::write16(uint32_t addr, uint16_t v) {
  addr &= 0xFFFFFF;
  if (addr + 2 <= RAM_SIZE) put16(addr, v);
  else unmapped(*this, addr, true);
}

// ---- input from host --------------------------------------------------------

void Machine::mouse_move(int dx, int dy) {
  // Queued and fed at most MOUSE_STEP counts per frame (mouse_frame):
  // JOY0DAT's counters are 8 bits and the game reads signed deltas, so a
  // jump of 128+ counts between two reads would move it the wrong way.
  pending_mouse_x_ += dx;
  pending_mouse_y_ += dy;
}

void Machine::mouse_frame() {
  constexpr int MOUSE_STEP = 32;
  const int dx = std::clamp(pending_mouse_x_, -MOUSE_STEP, MOUSE_STEP);
  const int dy = std::clamp(pending_mouse_y_, -MOUSE_STEP, MOUSE_STEP);
  if (!dx && !dy) return;
  pending_mouse_x_ -= dx;
  pending_mouse_y_ -= dy;
  hle_->mouse_move(dx, dy);
}

void Machine::mouse_button(int button, bool down) {
  hle_->mouse_button(button, down);
}

void Machine::key_event(uint8_t rawkey, bool down) { hle_->key_event(rawkey, down); }

// ---- copper + display -------------------------------------------------------

void Machine::render_line(int line) {
  uint32_t* out = nullptr;
  int fy = line - fb_vstart();
  if (fy >= 0 && fy < FB_H) out = &fb_[size_t(fy) * FB_W];

  int vstart = dn_.diwstrt_ >> 8;
  int vstop = (dn_.diwstop_ >> 8) | ((dn_.diwstop_ & 0x8000) ? 0 : 0x100);
  int hstart = dn_.diwstrt_ & 0xFF;
  int hstop = (dn_.diwstop_ & 0xFF) | 0x100;
  int nplanes = (dn_.bplcon0_ >> 12) & 7;
  bool hires = dn_.bplcon0_ & 0x8000;
  bool ham = dn_.bplcon0_ & 0x0800;
  bool dpf = dn_.bplcon0_ & 0x0400;
  if (hires) nplanes = std::min(nplanes, 4);
  bool dma = display_dma_;
  bool active = dma && nplanes > 0 && line >= vstart && line < vstop;

  uint32_t bg = rgb12(dn_.color_[0]);
  if (!out) {
    // Still consume bitplane data so pointers advance correctly.
    if (active) {
      int nwords = hires ? (dn_.ddfstop_ - dn_.ddfstrt_) / 4 + 2 : (dn_.ddfstop_ - dn_.ddfstrt_) / 8 + 1;
      for (int p = 0; p < nplanes; ++p)
        dn_.bplpt_[p] += uint32_t(nwords * 2 + ((p & 1) ? dn_.bpl2mod_ : dn_.bpl1mod_));
    }
    return;
  }
  for (int x = 0; x < FB_W; ++x) out[x] = bg;
  if (!active) return;

  int nwords = hires ? (dn_.ddfstop_ - dn_.ddfstrt_) / 4 + 2 : (dn_.ddfstop_ - dn_.ddfstrt_) / 8 + 1;
  if (nwords <= 0 || nwords > 64) nwords = 20;
  static uint8_t pix[64 * 16 + 32];
  int npix = nwords * 16;
  std::memset(pix, 0, sizeof pix);
  int delay1 = dn_.bplcon1_ & 15, delay2 = (dn_.bplcon1_ >> 4) & 15;
  // Planar to chunky, 8 pixels at a time: expand[b] has byte i = 1 when
  // bit 7 - i of b is set (the leftmost pixel is the most significant bit).
  static const auto expand = [] {
    std::array<uint64_t, 256> t{};
    for (int b = 0; b < 256; ++b)
      for (int i = 0; i < 8; ++i)
        if (b & (0x80 >> i)) t[size_t(b)] |= uint64_t(1) << (8 * i);  // little-endian bytes
    return t;
  }();
  auto or8 = [](uint8_t* dst, uint64_t v) {
    uint64_t d;
    std::memcpy(&d, dst, 8);
    d |= v;
    std::memcpy(dst, &d, 8);
  };
  for (int p = 0; p < nplanes; ++p) {
    uint32_t pt = dn_.bplpt_[p];
    int delay = (p & 1) ? delay2 : delay1;
    const uint8_t* line_bytes = bytes(pt, uint32_t(nwords * 2));  // the plane's words of this line
    for (int w = 0; w < nwords; ++w) {
      uint16_t data = uint16_t(line_bytes[w * 2] << 8 | line_bytes[w * 2 + 1]);
      if (!data) continue;
      uint8_t* dst = &pix[w * 16 + delay];
      or8(dst, expand[data >> 8] << p);
      or8(dst + 8, expand[data & 0xFF] << p);
    }
    dn_.bplpt_[p] = pt + uint32_t(nwords * 2 + ((p & 1) ? dn_.bpl2mod_ : dn_.bpl1mod_));
  }
  // first displayed pixel position (lores pixels, beam coordinates)
  int x0 = hires ? dn_.ddfstrt_ * 2 + 9 : dn_.ddfstrt_ * 2 + 17;
  uint32_t ham_col = dn_.color_[0];
  int step = hires ? 2 : 1;
  const bool ham_mode = ham && !hires && nplanes >= 6;
  // The colour of each pixel value for this line (all modes but HAM).
  uint32_t lut[64];
  if (!ham_mode) {
    for (int c = 0; c < 64; ++c) {
      if (dpf) {
        int pf1 = (c & 1) | ((c >> 1) & 2) | ((c >> 2) & 4);
        int pf2 = ((c >> 1) & 1) | ((c >> 2) & 2) | ((c >> 3) & 4);
        bool pf2pri = dn_.bplcon2_ & 0x40;
        int idx;
        if (pf1 && pf2) idx = pf2pri ? pf2 + 8 : pf1;
        else if (pf1) idx = pf1;
        else if (pf2) idx = pf2 + 8;
        else idx = 0;
        lut[c] = rgb12(dn_.color_[idx]);
      } else if (nplanes == 6 && !hires) {  // EHB
        uint16_t col = dn_.color_[c & 31];
        if (c & 32) col = (col >> 1) & 0x777;
        lut[c] = rgb12(col);
      } else {
        lut[c] = rgb12(dn_.color_[c & 31]);
      }
    }
  }
  if (!ham_mode) {
    // The visible span (display window and framebuffer) in beam pixels.
    const int lo = std::max({hstart, FB_HSTART, x0});
    const int hi = std::min({hstop, FB_HSTART + FB_W, x0 + npix / step});
    for (int hx = lo; hx < hi; ++hx) out[hx - FB_HSTART] = lut[pix[(hx - x0) * step] & 63];
    return;
  }
  for (int i = 0; i < npix; i += step) {
    int hx = x0 + (hires ? i / 2 : i);
    uint8_t c = pix[i];
    uint32_t rgb;
    if (!ham_mode) {
      rgb = lut[c & 63];
    } else {
      int ctl = (c >> 4) & 3, val = c & 15;
      switch (ctl) {
        case 0: ham_col = dn_.color_[val]; break;
        case 1: ham_col = (ham_col & 0xFF0) | uint32_t(val); break;
        case 2: ham_col = (ham_col & 0x0FF) | uint32_t(val) << 8; break;
        case 3: ham_col = (ham_col & 0xF0F) | uint32_t(val) << 4; break;
      }
      rgb = rgb12(uint16_t(ham_col));
    }
    if (hx < hstart || hx >= hstop) continue;
    int fx = hx - FB_HSTART;
    if (fx >= 0 && fx < FB_W) out[fx] = rgb;
  }
}

// The frame, line by line: each shown screen's registers from the line
// before it (its colours too), no planes from its end on (as the copper
// list MrgCop made did; bitplane DMA on once a display was made).
void Machine::render_frame() {
  const auto& bands = hle_->bands();
  const bool display = display_dma_;
  if (display) dn_.bplcon0_ = 0x0200;
  size_t next = 0;
  bool in_band = false;
  for (int line = 0; line < lines_per_frame(); ++line) {
    while (display && next < bands.size()) {
      const Hle::Band& b = bands[next];
      if (!in_band) {
        if (line < b.wait) break;
        dn_.diwstrt_ = b.diwstrt;
        dn_.diwstop_ = b.diwstop;
        dn_.ddfstrt_ = b.ddfstrt & 0xFC;
        dn_.ddfstop_ = b.ddfstop & 0xFC;
        dn_.bplcon1_ = 0;
        dn_.bplcon2_ = 0x24;
        for (int p = 0; p < 6; ++p)
          if (b.planes_set & (1 << p)) dn_.bplpt_[p] = b.planes[p] & 0xFFFFFE;
        dn_.bpl1mod_ = int16_t(b.bpl1mod & 0xFFFE);
        dn_.bpl2mod_ = int16_t(b.bpl2mod & 0xFFFE);
        for (int i = 0; i < b.colour_count; ++i) dn_.color_[i] = b.colours[i] & 0xFFF;
        dn_.bplcon0_ = b.bplcon0;
        in_band = true;
      }
      if (line < b.end) break;
      dn_.bplcon0_ = 0x0200;
      in_band = false;
      ++next;
    }
    render_line(line);
  }
}

// ---- blitter ------------------------------------------------------------------

void Machine::blit(uint16_t size) {
  Blitter& bl = blitter_;
  for (uint32_t& p : bl.pt) p &= 0xFFFFFE;  // even addresses, as the pointer registers held them
  for (int16_t& m : bl.mod) m = int16_t(m & 0xFFFE);
  int h = size >> 6;
  int w = size & 0x3F;
  if (h == 0) h = 1024;
  if (w == 0) w = 64;
  if (bl.con1 & 1) {
    blit_line(size);
  } else {
    bool useA = bl.con0 & 0x800, useB = bl.con0 & 0x400;
    bool useC = bl.con0 & 0x200, useD = bl.con0 & 0x100;
    uint8_t lf = uint8_t(bl.con0);
    int ash = bl.con0 >> 12, bsh = bl.con1 >> 12;
    bool desc = bl.con1 & 2;
    bool fill = bl.con1 & 0x18;
    bool efe = bl.con1 & 0x10;
    int inc = desc ? -2 : 2;
    uint16_t aold = 0, bold = 0;
    bool zero = true;
    uint32_t& apt = bl.pt[0];
    uint32_t& bpt = bl.pt[1];
    uint32_t& cpt = bl.pt[2];
    uint32_t& dpt = bl.pt[3];
    // The hardware's pipeline: a word's result is written to D after the
    // sources of the next word are read (one word late), so a blit whose
    // line wraps onto the start of the next one (a rectangle past the
    // bitmap's right edge) reads the next line's first word before this
    // line's last word lands on it.
    bool pending = false;
    uint32_t pending_at = 0;
    uint16_t pending_d = 0;
    auto flush = [&] {
      if (pending) put16(pending_at, pending_d);
      pending = false;
    };
    for (int y = 0; y < h; ++y) {
      bool fc = bl.con1 & 4;  // FCI
      for (int x = 0; x < w; ++x) {
        if (useA) { bl.dat[0] = bytes16(apt); apt += uint32_t(inc); }
        if (useB) { bl.dat[1] = bytes16(bpt); bpt += uint32_t(inc); }
        if (useC) { bl.dat[2] = bytes16(cpt); cpt += uint32_t(inc); }
        flush();  // the previous word's result, after this word's reads
        uint16_t a = bl.dat[0];
        if (x == 0) a &= bl.afwm;
        if (x == w - 1) a &= bl.alwm;
        uint16_t as, bs;
        if (!desc) {
          as = uint16_t(((uint32_t(aold) << 16) | a) >> ash);
          bs = uint16_t(((uint32_t(bold) << 16) | bl.dat[1]) >> bsh);
        } else {
          as = uint16_t(((uint32_t(a) << 16) | aold) >> (16 - ash));
          bs = uint16_t(((uint32_t(bl.dat[1]) << 16) | bold) >> (16 - bsh));
        }
        aold = a;
        bold = bl.dat[1];
        uint16_t d = blitter_minterm(lf, as, bs, bl.dat[2]);
        if (fill) d = blitter_fill_word(d, fc, efe);
        if (d) zero = false;
        if (useD) {
          pending = true;
          pending_at = dpt;
          pending_d = d;
          dpt += uint32_t(inc);
        }
      }
      if (useA) apt += uint32_t(desc ? -bl.mod[0] : bl.mod[0]);
      if (useB) bpt += uint32_t(desc ? -bl.mod[1] : bl.mod[1]);
      if (useC) cpt += uint32_t(desc ? -bl.mod[2] : bl.mod[2]);
      if (useD) dpt += uint32_t(desc ? -bl.mod[3] : bl.mod[3]);
    }
    flush();
    bl.zero = zero;
  }
  raise(INT_BLIT);
  // The blit happens instantly; charge a rough DMA cost to the CPU.
  cpu.cycles += int64_t(w) * h * 2;
}

void Machine::blit_line(uint16_t size) {
  Blitter& bl = blitter_;
  int len = size >> 6;
  if (len == 0) len = 1024;
  uint8_t lf = uint8_t(bl.con0);
  int ashift = bl.con0 >> 12;
  int bshift = bl.con1 >> 12;
  bool sing = bl.con1 & 2;
  bool sud = bl.con1 & 0x10, sul = bl.con1 & 0x08, aul = bl.con1 & 0x04;
  bool sign = bl.con1 & 0x40;
  int16_t err = int16_t(bl.pt[0] & 0xFFFF);
  uint32_t cpt = bl.pt[2];
  uint32_t dpt = bl.pt[3];
  uint16_t texture = bl.dat[1];
  bool onedot = false;
  bool zero = true;
  uint16_t ahold = bl.dat[0] & bl.afwm;

  auto incx = [&]() { if (++ashift == 16) { ashift = 0; cpt += 2; } };
  auto decx = [&]() { if (ashift-- == 0) { ashift = 15; cpt -= 2; } };
  auto incy = [&]() { cpt += uint32_t(bl.mod[2]); onedot = false; };
  auto decy = [&]() { cpt -= uint32_t(bl.mod[2]); onedot = false; };

  for (int i = 0; i < len; ++i) {
    uint16_t c = (bl.con0 & 0x200) ? bytes16(cpt) : bl.dat[2];
    uint16_t a = uint16_t(ahold >> ashift);
    bool draw = !(sing && onedot);
    uint16_t b = ((texture >> bshift) & 1) ? 0xFFFF : 0;
    if (!draw) a = 0;
    uint16_t d = blitter_minterm(lf, a, b, c);
    onedot = true;
    if (d) zero = false;
    uint32_t write_at = dpt;
    // step
    if (!sign) err = int16_t(err + bl.mod[0]);
    else err = int16_t(err + bl.mod[1]);
    if (!sign) {
      if (sud) { if (sul) decy(); else incy(); }
      else { if (sul) decx(); else incx(); }
    }
    if (sud) { if (aul) decx(); else incx(); }
    else { if (aul) decy(); else incy(); }
    sign = err < 0;
    bshift = (bshift - 1) & 15;
    if ((bl.con0 & 0x100) && draw) {
      put16(write_at, d);
    }
    dpt = cpt;
  }
  bl.pt[0] = (bl.pt[0] & 0xFFFF0000) | uint16_t(err);
  bl.pt[2] = cpt;
  bl.pt[3] = dpt;
  bl.con0 = uint16_t((bl.con0 & 0x0FFF) | (ashift << 12));
  bl.con1 = uint16_t((bl.con1 & 0x0FBF) | (bshift << 12) | (sign ? 0x40 : 0));
  bl.zero = zero;
}

// ---- Paula --------------------------------------------------------------------

void Machine::audio_start(int ch) {
  AudioChannel& a = aud_[ch];
  if (a.active) return;
  a.active = true;
  a.pt = a.lc;
  a.remaining = a.len ? a.len : 0x10000;
  a.byte_index = 0;
  a.phase = 0;
  raise(uint16_t(INT_AUD0 << ch));
}

void Machine::audio_advance(double ticks, int16_t* out, int frames) {
  for (int f = 0; f < frames; ++f) {
    int l = 0, r = 0;
    for (int ch = 0; ch < 4; ++ch) {
      AudioChannel& a = aud_[ch];
      if (a.active) {
        int per = a.per < 124 ? 124 : a.per;
        a.phase += ticks;
        while (a.phase >= per) {
          a.phase -= per;
          uint16_t w = bytes16(a.pt);
          a.sample = int8_t(a.byte_index ? (w & 0xFF) : (w >> 8));
          if (++a.byte_index == 2) {
            a.byte_index = 0;
            a.pt += 2;
            if (--a.remaining == 0) {
              // audio.device: next queued write, or stop (latched before
              // the reload, like the hardware's LC/LEN double buffer).
              hle_->audio_block_end(ch);
              if (!a.active) break;
              a.pt = a.lc;
              a.remaining = a.len ? a.len : 0x10000;
              raise(uint16_t(INT_AUD0 << ch));
            }
          }
        }
      }
      int vol = a.vol > 64 ? 64 : a.vol;
      int s = a.active ? a.sample * vol : 0;
      if (ch == 0 || ch == 3) l += s; else r += s;
    }
    // A500 output stage: fixed one-pole RC low-pass at ~4.4 kHz.
    lp_l_ += lp_k_ * (double(l) - lp_l_);
    lp_r_ += lp_k_ * (double(r) - lp_r_);
    // 2 channels × 127 × 64 → scale to 16 bit
    out[f * 2] = int16_t(std::clamp(int(lp_l_ * 2), -32768, 32767));
    out[f * 2 + 1] = int16_t(std::clamp(int(lp_r_ * 2), -32768, 32767));
  }
}

void Machine::audio_line() {
  int rate = host_ ? host_->audio_rate() : 48000;
  lp_k_ = 1.0 - std::exp(-2.0 * 3.14159265358979 * 4400.0 / rate);
  audio_frac_ += double(rate) / (lines_per_frame() * field_rate());
  int n = int(audio_frac_);
  if (n <= 0) return;
  audio_frac_ -= n;
  size_t off = audio_buf_.size();
  audio_buf_.resize(off + size_t(n) * 2);
  audio_advance(paula_clock() / rate, &audio_buf_[off], n);
}

}  // namespace amiga
