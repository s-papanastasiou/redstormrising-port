// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The game's files from the WHDLoad install archive
// (RedStormRising_v1.0_1603.lha) or the two disks of v843.02 (ADF images):
// the files the game uses, read straight from them, so a player does not
// have to unpack anything.
#pragma once

#include <string>
#include <vector>

namespace host {

// Whether `dir` holds the game's files (by name and size).
bool has_game_files(const std::string& dir);

// The game's files from `sources` (an .lha, or the ADFs of both disks; the
// kind is told by the contents), each checked against the WHDLoad
// install's (another build is refused before anything is written), written
// to `out_dir` with their names in the install's data/ directory. The
// disks' cracked RedStart gets its original two bytes back. False with
// `err` set on failure.
bool extract_game_files(const std::vector<std::string>& sources, const std::string& out_dir, std::string& err);

}  // namespace host
