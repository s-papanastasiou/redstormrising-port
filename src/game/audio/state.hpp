// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The sound code's state (Storm, Title, RedStart and Red; RedStorm has
// none), kept in C++ with a copy per module (mem/module_state.hpp). Sound
// programs are named by their address (a channel's program); the ones the
// game writes (the engine programs, set_tone's two words inside an effect's
// program) are fields here, the others are read from the constant data
// (program_word).
#pragma once

#include "game/audio/vars.hpp"
#include "game/mem/module_state.hpp"

namespace game::audio {

inline constexpr int kSampleSlots = 20;

struct State {
  int16_t io_result;  // last OpenDevice / CheckIO result
  Channel channels[kChannels];  // [4] the program players
  uint16_t sound_clock;  // advanced every frame
  int16_t engine_speed;  // 0: engine noise off
  int16_t quiet_mode;  // channels 1-3 kept quiet
  int16_t engine_running;  // -1: the engine program is restarted when idle
  Sample samples[kSampleSlots];  // [kSampleSlots] by sample number
  Ptr<uint8_t> preload_block;  // all samples in one block (when preloaded)
  Ptr<be32> sample_offsets;  // [] file offset per sample
  int16_t samples_preloaded;
  int16_t audio_open;  // -1 when the device is open
  int16_t sound_enabled;  // 0 while the player must not run
  uint8_t ticker_installed;
  uint8_t pal_ticks;  // PAL: an extra music tick
  uint8_t tick_countdown;  // ... every 6th frame
  be32 frame_counter;  // (big-endian: read through its address) +1 a frame (the vertical-blank server); its address is the modules' frame timer
  EngineProgram engine_main;  // the engine noise (engine_program: its handle)
  EngineProgram engine_a, engine_b;  // the alternative engine programs
  sbe16 tones[2];  // (big-endian: read as program words) set_tone's two words (tone_words: their handle)
};

// Each module's copy, and the running module's.
extern State states[5];
State& state();

inline constexpr int32_t X = mem::module_state::kAbsent;
#define AUDIO_BE(member, size, ...) MODULE_FIELD_BE(State, member, size, __VA_ARGS__)
#define AUDIO_N(member, size, elem, ...) MODULE_FIELD_N(State, member, size, elem, __VA_ARGS__)
// Storm's offsets plus the module's shift of the library's two blocks.
inline constexpr mem::module_state::FieldAt kFields[] = {
    AUDIO_N(io_result, 2, 2, 9052, X, 3940, 28704, 16376),
    AUDIO_BE(channels, 160, 9056, X, 3944, 28708, 16380),
    AUDIO_N(sound_clock, 2, 2, 9216, X, 4104, 28868, 16540),
    AUDIO_N(engine_speed, 2, 2, 9218, X, 4106, 28870, 16542),
    AUDIO_N(quiet_mode, 2, 2, 9220, X, 4108, 28872, 16544),
    AUDIO_N(engine_running, 2, 2, 9222, X, 4110, 28874, 16546),
    AUDIO_BE(samples, 200, 9224, X, 4112, 28876, 16548),
    AUDIO_BE(preload_block, 4, 9718, 10342, 4606, 29370, 17042),
    AUDIO_BE(sample_offsets, 4, -11438, 1054, -23318, -16778, 10382),
    AUDIO_N(samples_preloaded, 2, 2, -11434, 1058, -23314, -16774, 10386),
    AUDIO_N(audio_open, 2, 2, -13310, X, -25190, -18650, 8510),
    AUDIO_N(sound_enabled, 2, 2, -13312, X, -25192, -18652, 8508),
    AUDIO_BE(ticker_installed, 1, 8996, X, 4728, 29492, 16320),
    AUDIO_BE(pal_ticks, 1, 8972, X, 4704, 29468, 16296),
    AUDIO_BE(tick_countdown, 1, 8973, X, 4705, 29469, 16297),
    AUDIO_BE(frame_counter, 4, 8968, X, 4700, 29464, 16292),
    // The library's Block::Bss shift: title -11880, redstart -5340, red 21820.
    AUDIO_BE(engine_main, 12, -13308, X, -25188, -18648, 8512),
    AUDIO_BE(engine_a, 12, -12588, X, -24468, -17928, 9232),
    AUDIO_BE(engine_b, 12, -12574, X, -24454, -17914, 9246),
    AUDIO_BE(tones, 4, -12278, X, -24158, -17618, 9542),
};
#undef AUDIO_BE

}  // namespace game::audio
