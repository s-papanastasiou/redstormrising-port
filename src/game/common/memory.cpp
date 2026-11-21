// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The C runtime's memory allocator (Aztec C, in Storm, RedStorm and
// Title; RedStart and Red only free its blocks at the end). Its blocks
// come from AllocMem as before, so the heap's layout and the addresses it
// returns are the original's; its own records (the blocks' 12-byte
// headers, the free pieces' links, the size word in front of each malloc)
// are kept in C++.
#include "game/common/memory.hpp"
#include "game/common/vars.hpp"
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/common/state.hpp"
#include "game/mem/state.hpp"

#include <cstdio>
#include <cstdlib>

namespace game::common {

namespace {
constexpr int kMaxPieces = 64, kMaxBlocks = 64, kMaxSized = 64;

// The free list, in address order (the original's starts at a variable
// of the initialised data: it is kept while the module stays loaded, even
// across its runs, when its pieces are in blocks the last run's end gave
// back; the original then reused that memory too).
struct FreeList {
  struct Piece {
    uint32_t addr;
    int32_t bytes;
  } pieces[kMaxPieces];
  int32_t count;
};
// The blocks, in allocation order, and the malloc'd memory's sizes (the
// word in front of each): for the run (the BSS).
struct Blocks {
  struct Block {
    uint32_t addr, bytes;
  } blocks[kMaxBlocks];
  int32_t count;
  struct Sized {
    uint32_t addr;  // of the size word (what the deferred free names)
    uint16_t bytes;
  } sized[kMaxSized];
  int32_t sized_count;
};
FreeList g_free[3];  // storm, redstorm, title
Blocks g_blocks[3];

FreeList& free_list_here() { return g_free[runtime::module_index()]; }
Blocks& blocks_here() { return g_blocks[runtime::module_index()]; }

[[noreturn]] void full(const char* what) {
  std::fprintf(stderr, "the C allocator's %s are full\n", what);
  std::abort();
}

void insert_piece(FreeList& f, int32_t at, uint32_t addr, int32_t bytes) {
  if (f.count == kMaxPieces) full("free pieces");
  for (int32_t k = f.count; k > at; --k) f.pieces[k] = f.pieces[k - 1];
  f.pieces[at] = {addr, bytes};
  ++f.count;
}
void erase_piece(FreeList& f, int32_t at) {
  for (int32_t k = at; k + 1 < f.count; ++k) f.pieces[k] = f.pieces[k + 1];
  --f.count;
}

// The size word written in front of malloc'd memory, and read back.
void set_size(uint32_t addr, uint16_t bytes) {
  Blocks& b = blocks_here();
  for (int32_t k = 0; k < b.sized_count; ++k)
    if (b.sized[k].addr == addr) {
      b.sized[k].bytes = bytes;
      return;
    }
  if (b.sized_count == kMaxSized) full("size words");
  b.sized[b.sized_count++] = {addr, bytes};
}
uint16_t take_size(uint32_t addr) {
  Blocks& b = blocks_here();
  for (int32_t k = 0; k < b.sized_count; ++k)
    if (b.sized[k].addr == addr) {
      const uint16_t bytes = b.sized[k].bytes;
      b.sized[k] = b.sized[--b.sized_count];
      return bytes;
    }
  std::fprintf(stderr, "free of %06x: not malloc'd\n", addr);
  std::abort();
}

int32_t rounded(int32_t bytes) { return (std::max(bytes, kMinPiece) + 3) & ~3; }

// The free list's variable (and its pieces) moved: the free list is
// initialised data, the blocks are BSS.
constexpr const char* kNames[3] = {"storm", "redstorm", "title"};
template <int M>
void load_free_list() {
  const uint8_t* image = mem::state::loaded_image(free_list.off[M]);
  if (image[0] | image[1] | image[2] | image[3]) {  // the loaded image's list is empty
    std::fprintf(stderr, "%s: the C free list is not empty in the image\n", kNames[M]);
    std::abort();
  }
  g_free[M] = FreeList{};
}
bool register_heaps() {
  static mem::state::Range moved[3][1];
  for (int m = 0; m < 3; ++m) moved[m][0] = {free_list.off[m], 4};
  constexpr void (*kLoad[3])() = {load_free_list<0>, load_free_list<1>, load_free_list<2>};
  constexpr void (*kReset[3])() = {[] { g_blocks[0] = Blocks{}; }, [] { g_blocks[1] = Blocks{}; },
                                   [] { g_blocks[2] = Blocks{}; }};
  for (int m = 0; m < 3; ++m) {
    mem::state::add({m, kNames[m], nullptr, kLoad[m], &g_free[m], uint32_t(sizeof(FreeList)), moved[m], 1, nullptr, 0});
    mem::state::add({m, kNames[m], kReset[m], nullptr, &g_blocks[m], uint32_t(sizeof(Blocks)), nullptr, 0, nullptr, 0});
  }
  return true;
}
[[maybe_unused]] const bool g_registered = register_heaps();
}  // namespace

// $33c14 (RedStorm $23454, Title $24f9c): a block of `bytes` from
// AllocMem (with its 12-byte header), at the end of the block list.
// Returns the memory after the header, or 0. (Its bytes are in the host
// arena, item 4e.5: what malloc hands out, the animation scripts.)
uint32_t alloc_block(int32_t bytes) {
  bytes += kBlockHeader;
  const uint32_t block = os::alloc_data(uint32_t(bytes));
  if (!block) return 0;
  Blocks& b = blocks_here();
  if (b.count == kMaxBlocks) full("blocks");
  b.blocks[b.count++] = {block, uint32_t(bytes)};
  mem::state::move_address_range(block, kBlockHeader);
  if (!state().block_list) state().block_list = block;
  state().block_list_tail = block;
  if (!state().first_block) state().first_block = block;
  return block + kBlockHeader;
}
ORIGINAL(alloc_block, 0x33c14, 0x23454, 0x24f9c, 0, 0);

// $33be4 (and a copy in every module): all blocks freed (at the end).
// (RedStart and Red have none: they only run the end of the C runtime.)
void free_blocks() {
  const int m = runtime::module_index();
  if (m < 3) {
    Blocks& b = g_blocks[m];
    for (int32_t k = 0; k < b.count; ++k) {
      mem::state::release_address_range(b.blocks[k].addr, kBlockHeader);
      os::free_mem(b.blocks[k].addr, b.blocks[k].bytes);
    }
    b.count = 0;
  }
  state().block_list_tail = 0;
  state().block_list = 0;
}
ORIGINAL(free_blocks, 0x33be4, 0x23424, 0x24f6c, 0x282ac, 0x2eabc);

// $341e0 (RedStorm $2398c, Title $254d4): the memory p (bytes long) into
// the free list, merged with the free pieces it touches. Returns 0, or
// -1 when it overlaps a free piece (nothing changes).
int32_t add_free(uint32_t p, int32_t bytes) {
  if (bytes <= 0) return -1;
  bytes = rounded(bytes);
  const uint32_t end = p + uint32_t(bytes);
  FreeList& f = free_list_here();
  state().free_bytes = state().free_bytes + bytes;
  for (int32_t k = 0; k < f.count; ++k) {
    auto& cur = f.pieces[k];
    const uint32_t cur_end = cur.addr + uint32_t(cur.bytes);
    if (cur.addr > end) {  // before cur, apart
      insert_piece(f, k, p, bytes);
      return 0;
    }
    if (cur.addr == end) {  // just before cur: one piece
      cur = {p, cur.bytes + bytes};
      return 0;
    }
    if (p < cur_end) {
      state().free_bytes = state().free_bytes - bytes;
      return -1;
    }
    if (p == cur_end) {  // just after cur
      const bool has_next = k + 1 < f.count;
      if (has_next && f.pieces[k + 1].addr < end) {
        state().free_bytes = state().free_bytes - bytes;
        return -1;
      }
      cur.bytes += bytes;
      if (has_next && f.pieces[k + 1].addr == end) {
        cur.bytes += f.pieces[k + 1].bytes;
        erase_piece(f, k + 1);
      }
      return 0;
    }
  }
  insert_piece(f, f.count, p, bytes);  // at the end
  return 0;
}
ORIGINAL(add_free, 0x341e0, 0x2398c, 0x254d4, 0, 0);

// $33694 (RedStorm $22f80, Title $24aa0): `bytes` from the first free
// piece that holds them (the rest stays free if at least 8 bytes); with
// none, a new block (a multiple of the growth unit) joins the free list.
uint32_t lmalloc(int32_t bytes) {
  if (bytes <= 0) return 0;
  bytes = rounded(bytes);
  FreeList& f = free_list_here();
  for (int32_t k = 0; k < f.count; ++k) {
    auto& cur = f.pieces[k];
    const int32_t have = cur.bytes;
    if (have < bytes) continue;
    const uint32_t at = cur.addr;
    if (have == bytes) {
      erase_piece(f, k);
      state().free_bytes = state().free_bytes - bytes;
      return at;
    }
    if (have - bytes >= kMinPiece) {
      cur = {at + uint32_t(bytes), have - bytes};
      state().free_bytes = state().free_bytes - bytes;
      return at;
    }
  }
  const int32_t unit = *growth_unit;
  const int32_t grow = ((((bytes + unit - 1) / unit) * unit + kMinPiece) + 3) & ~3;
  const uint32_t block = alloc_block(grow);
  if (!block) return 0;
  add_free(block, grow);
  return lmalloc(bytes);
}
ORIGINAL(lmalloc, 0x33694, 0x22f80, 0x24aa0, 0, 0);

// $34300 (RedStorm $23aa4, Title $255ec): `bytes` with a size word in
// front (the memory freed last is given back first). Returns 0 for none.
uint32_t c_malloc(uint16_t bytes) {
  if (state().deferred_free) {
    add_free(state().deferred_free, take_size(state().deferred_free));
    state().deferred_free = 0;
  }
  if (!bytes) return 0;
  const uint16_t total = uint16_t(bytes + 2);
  const uint32_t p = lmalloc(total);
  if (!p) return 0;
  set_size(p, total);
  return p + 2;
}
ORIGINAL(c_malloc, 0x34300, 0x23aa4, 0x255ec, 0, 0);

// $33774 (Title $24b84): p freed at the next malloc (the one freed before
// it now).
void c_free(uint32_t p) {
  if (!p) return;
  c_malloc(0);
  state().deferred_free = p - 2;
}
ORIGINAL(c_free, 0x33774, 0, 0x24b84, 0, 0);

}  // namespace game::common
