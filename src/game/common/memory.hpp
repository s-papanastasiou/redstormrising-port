// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The C runtime's memory allocator (memory.cpp).
#pragma once

#include <cstdint>

namespace game::common {

uint32_t c_malloc(uint16_t bytes);
void c_free(uint32_t p);
uint32_t lmalloc(int32_t bytes);
int32_t add_free(uint32_t p, int32_t bytes);
uint32_t alloc_block(int32_t bytes);
void free_blocks();
// Memory the original allocated (cleared) for an OS object the game now
// keeps in C++ (ports, requests, module handles): still taken
// from the heap so its layout stays the original's, its address naming
// the object; the trace checks nothing reaches it while it lives.

}  // namespace game::common
