// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Host services the game uses instead of AmigaOS's message ports and
// devices. Each costs the time
// the OS call it replaces took, so the recordings stay the same.
#pragma once

#include <functional>

#include <cstdint>

namespace game::platform {

// The next input event of the active window (what GetMsg on its UserPort
// gave): an IDCMP class, the code and the qualifier. False: none (a
// waiting call: under --det-timing a scanline passes, as for GetMsg).
struct InputEvent {
  uint32_t cls;
  uint16_t code, qualifier;
};
bool next_input(InputEvent& event);

// The keys held down (what keyboard.device's KBD_READMATRIX gave through
// DoIO, a waiting call): a bit per raw key code, `length` bytes into the
// memory image at `to`.
void read_key_matrix(uint8_t* to, uint32_t length);
// The joystick in port 2: directions and fire.
struct Joystick {
  bool up, down, left, right, fire;
};
Joystick joystick();

// An input event written to input.device (Storm moves the pointer while
// aiming): the class (raw mouse: relative; pointer position: absolute),
// the qualifier, and x, y in half pixels: Intuition's pointer moves, as
// on the Amiga (the emulator before step 4 completed IND_WRITEEVENT
// without effect). A waiting call, as DoIO.
void write_input_event(uint8_t cls, uint16_t qualifier, int16_t x, int16_t y);

// The system time (timer.device's TR_GETSYSTIME through DoIO, a waiting
// call): seconds and microseconds of the emulated machine's clock.
void system_time(uint32_t& seconds, uint32_t& micros);
inline constexpr uint64_t kMicrosPerSecond = 1000000;

// audio.device on the channels of `mask` (a request each, sent with
// BeginIO): ADCMD_PERVOL, CMD_FLUSH, CMD_WRITE (`key` names the write;
// cycles 0: until flushed); CheckIO (a waiting call) whether the write
// `key` has completed, and WaitIO on it.
void audio_set(uint8_t mask, uint16_t period, uint16_t volume);
void audio_flush(uint8_t mask);
void audio_write(uint32_t key, uint8_t mask, uint32_t data, uint32_t length, uint16_t period, uint16_t volume,
                 uint16_t cycles, bool pervol);
bool audio_check(uint32_t key);
void audio_wait(uint32_t key);

// The vertical-blank interrupt server (AddIntServer, RemIntServer): the
// host calls `server` at each vertical blank, in the module that installs
// it, at priority `pri`. `node` (the emulated address of the module's
// Interrupt node) names it. The server's result 0 lets the next one run.
void add_vblank_server(uint32_t node, int8_t pri, int32_t (*server)());
void rem_vblank_server(uint32_t node);

// Waits: dos Delay (ticks of 1/50 s of the machine's clock) and graphics
// WaitTOF (until the next frame).
void delay(int32_t ticks);
// --reference-terrain (tests): the running module's image as the reference
// recordings had it loaded (Storm at Machine::kReferenceStormBlock), or
// null: the image as loaded now.
const uint8_t* reference_image();
// A routine the original called through an address (the effect table, a
// module's start-up), with the time that call took.
void call_timed(const std::function<void()>& routine);
// Run `body` in module `module` loaded at `block` (a module's start-up).
void in_module(int module, uint32_t block, const std::function<void()>& body);
// The running module's load address (its tools' address $20000).
uint32_t module_block();
void wait_frame();
inline constexpr uint64_t kMicrosPerTick = 20000;

}  // namespace game::platform
