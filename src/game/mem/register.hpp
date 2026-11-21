// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Where a function was in the original, for routines that several modules
// have, each at its own address:
//
//   ORIGINAL(draw_line, 0x3138a, 0x2149a, 0x231d2, 0x234ca, 0x27432);
//
// Addresses in the tools' layout of storm, redstorm, title, redstart and
// red; 0 where a module does not have the routine. Since item 5a.3 these
// register nothing (no code is entered by address); they document the
// original, and tools/analysis/registrations.py reads them, so keep each
// on one line with literal addresses.
#pragma once

#include "runtime/runtime.hpp"

#define ORIGINAL(fn, storm, redstorm, title, redstart, red)
// An original routine whose work is done inside another function.
#define ORIGINAL_INSIDE(name, storm, redstorm, title, redstart, red)
