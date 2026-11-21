// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The launch screen (shown when the program starts without options): the
// CRT look, fullscreen or a window, then start the game or quit. Drawn
// with SDL only (its renderer and built-in 8x8 font).
#pragma once

#include <string>

namespace launcher {

struct Settings {
  std::string crt;          // a bundled preset's name ("crt-lottes-fast" ...); empty: none
  bool fullscreen = true;
  std::string error;        // shown instead of Start (no game files ...)
};

// Settings remembered from the last start (`path`; defaults if none).
Settings load(const std::string& path);
void save(const Settings& s, const std::string& path);

// The launch screen until the player starts (true, `s` updated) or quits.
bool run(Settings& s);

}  // namespace launcher
