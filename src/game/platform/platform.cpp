// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Host services (platform.hpp).
#include "game/platform/platform.hpp"

#include "amiga/hle.hpp"
#include "runtime/runtime.hpp"

namespace game::platform {

bool next_input(InputEvent& event) {
  bool got = false;
  runtime::g_machine->os_call(true, [&] {
    amiga::Hle::InputEvent e;
    got = runtime::g_machine->hle().next_input(e);
    if (got) event = {e.cls, e.code, e.qualifier};
  });
  if (got) runtime::g_machine->os_call(false, [] {});  // the ReplyMsg the game made
  return got;
}

void read_key_matrix(uint8_t* to, uint32_t length) {
  runtime::g_machine->os_call(true, [&] { runtime::g_machine->hle().read_key_matrix(to, length); });
}

Joystick joystick() {
  const amiga::Machine::Joystick j = runtime::g_machine->joystick();
  return {j.up, j.down, j.left, j.right, j.fire};
}

void write_input_event(uint8_t cls, uint16_t qualifier, int16_t x, int16_t y) {
  runtime::g_machine->os_call(true, [&] { runtime::g_machine->hle().pointer_event(cls, qualifier, x, y); });
}

void system_time(uint32_t& seconds, uint32_t& micros) {
  runtime::g_machine->os_call(true, [&] {
    const uint64_t us = runtime::g_machine->micros();
    seconds = uint32_t(us / kMicrosPerSecond);
    micros = uint32_t(us % kMicrosPerSecond);
  });
}

void audio_set(uint8_t mask, uint16_t period, uint16_t volume) {
  runtime::g_machine->os_call(false, [&] { runtime::g_machine->hle().audio_set(mask, period, volume); });
}
void audio_flush(uint8_t mask) {
  runtime::g_machine->os_call(false, [&] { runtime::g_machine->hle().audio_flush(mask); });
}
void audio_write(uint32_t key, uint8_t mask, uint32_t data, uint32_t length, uint16_t period, uint16_t volume,
                 uint16_t cycles, bool pervol) {
  runtime::g_machine->os_call(false, [&] {
    runtime::g_machine->hle().audio_write(key, mask, data, length, period, volume, cycles, pervol);
  });
}
bool audio_check(uint32_t key) {
  bool done = false;
  runtime::g_machine->os_call(true, [&] { done = runtime::g_machine->hle().audio_done(key); });
  return done;
}
void audio_wait(uint32_t key) {
  runtime::g_machine->os_call(true, [&] {
    runtime::g_machine->idle_until([key] { return runtime::g_machine->hle().audio_done(key); });
  });
}

void call_timed(const std::function<void()>& routine) { runtime::g_machine->call_timed(routine); }
void in_module(int module, uint32_t block, const std::function<void()>& body) {
  runtime::g_machine->with_module(module, block, body);
}
uint32_t module_block() { return runtime::g_machine->module_block(); }

const uint8_t* reference_image() {
  amiga::Machine& m = *runtime::g_machine;
  if (!m.reference_terrain) return nullptr;
  return m.image_as_loaded_at("storm", amiga::Machine::kReferenceStormBlock).data();
}

namespace {
constexpr int kVertb = 5;  // INTB_VERTB
}  // namespace

void add_vblank_server(uint32_t node, int8_t pri, int32_t (*server)()) {
  amiga::Machine& m = *runtime::g_machine;
  const uint32_t block = m.module_block();
  const int module = m.module_number();
  m.os_call(false, [&] {
    m.hle().add_host_server(kVertb, node, pri, [&m, block, module, server] {
      int32_t result = 0;
      m.with_module(module, block, [&] { result = server(); });
      return result;
    });
  });
}

void rem_vblank_server(uint32_t node) {
  runtime::g_machine->os_call(false, [&] { runtime::g_machine->hle().rem_server(kVertb, node); });
}

void delay(int32_t ticks) {
  amiga::Machine& m = *runtime::g_machine;
  m.os_call(true, [&] {
    const uint64_t until = m.micros() + uint64_t(uint32_t(ticks)) * kMicrosPerTick;
    m.idle_until([&m, until] { return m.micros() >= until; });
  });
}

void wait_frame() {
  amiga::Machine& m = *runtime::g_machine;
  m.os_call(true, [&] {
    const uint64_t frame = m.frame_count();
    m.idle_until([&m, frame] { return m.frame_count() != frame; });
  });
}

}  // namespace game::platform
