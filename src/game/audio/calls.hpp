// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Audio: the routines of the subsystem (audio.cpp).
#pragma once

#include <cstdint>

#include "game/common/strings.hpp"

namespace game::audio {


// audio.cpp
void start_sound(int16_t period, int16_t volume, int16_t channel);
int16_t close_audio(int16_t stage);
void open_audio();
void play_note(int16_t sample, int16_t period, int16_t volume, int16_t channel);
void sound_tick(int16_t channel);
void sound_program(uint32_t program, int16_t channel);
void engine_sound();
void sound_frame();
void set_engine(int16_t speed);
void silence();
void stop_ticker();
void silence_all();
void reset_channels();
void sound_mode(int16_t quiet);
void sound_on();
void engine_pitch(int16_t speed);
void play_effect(int16_t n);
void set_tone(int16_t v);
void load_sample(int16_t i);
void free_block(int16_t i);
void load_samples1();  // the groups of samples (each module its own)
void free_samples1();
void load_samples2();
void free_samples2();
void load_samples3();
void free_samples3();
void load_samples4();
void free_samples4();
void start_ticker();
// The running module's frame counter (what its frame timer points at; the
// vertical-blank server counts it).
uint32_t frames();
void sound_off();
void sound_start();
void engine_on();
void engine_off();

}  // namespace game::audio
