// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-FileCopyrightText: 2026 Ingo Ruhnke
// SPDX-License-Identifier: GPL-3.0-or-later

// audio.device on the Paula model (custom.cpp), with its requests kept on
// the host: the game calls the typed functions (audio_set, audio_flush,
// audio_write, audio_done) with a key per request.
//
// CMD_WRITE requests queue per channel; the playing one repeats its sample
// `cycles` times (0 = until flushed) and completes at the end. At each
// Paula block end (Machine::audio_advance) the next queued write's
// pointer/length/period/volume are latched before the reload, so
// consecutive writes play seamlessly as on the hardware.
#include "amiga/hle.hpp"

#include <algorithm>

namespace amiga {

namespace {
// IOAudio (1.3): IORequest (io_Unit = allocated channel mask), ioa_AllocKey
// 32, ioa_Data 34, ioa_Length 38, ioa_Period 42, ioa_Volume 44, ioa_Cycles 46.
constexpr uint32_t IO_UNIT = 24, IO_COMMAND = 28, IO_FLAGS = 30;
constexpr uint32_t IOA_DATA = 34, IOA_LENGTH = 38, IOA_PERIOD = 42, IOA_VOLUME = 44, IOA_CYCLES = 46;
constexpr uint16_t CMD_RESET = 1, CMD_WRITE = 3, CMD_STOP = 6, CMD_START = 7, CMD_FLUSH = 8, ADCMD_FREE = 9,
                   ADCMD_FINISH = 11, ADCMD_PERVOL = 12, ADCMD_ALLOCATE = 32;
constexpr uint8_t ADIOF_PERVOL = 0x10;
constexpr int8_t IOERR_ABORTED = -2;
}  // namespace

// Program channel `ch` with the write `key` (pointer, length, period,
// volume); the reload at the next block start uses them.
void Hle::audio_latch(int ch, uint32_t key) {
  const AudioRequest& r = audio_requests_[key];
  AudioChannel& a = m.audio_channel(ch);
  a.lc = r.data & ~1u;
  a.len = uint16_t(std::max<uint32_t>(1, r.length / 2));
  a.per = r.period;
  a.vol = uint16_t(std::min<int>(r.volume, 64));
}

void Hle::audio_start_next(int ch) {
  AudioChan& c = audio_[ch];
  if (c.queue.empty()) {
    c.playing = 0;
    m.audio_stop(ch);
    return;
  }
  c.playing = c.queue.front();
  c.queue.pop_front();
  c.cycles_left = audio_requests_[c.playing].cycles;
  audio_latch(ch, c.playing);
  m.audio_start(ch);  // from lc (unless playing: then at the next block)
}

bool Hle::audio_queued(uint32_t key) const {
  for (int k = 0; k < 4; ++k)
    if (audio_[k].playing == key || std::find(audio_[k].queue.begin(), audio_[k].queue.end(), key) !=
                                        audio_[k].queue.end())
      return true;
  return false;
}

void Hle::audio_complete(uint32_t key, int8_t error) {
  AudioRequest& r = audio_requests_[key];
  r.done = true;
  r.error = error;
}

// Called by Machine::audio_advance when channel `ch` finished a block,
// before it reloads pointer and length.
void Hle::audio_block_end(int ch) {
  AudioChan& c = audio_[ch];
  if (!c.playing) return;
  if (c.cycles_left == 0) return;      // repeat until flushed
  if (--c.cycles_left > 0) return;
  const uint32_t done = c.playing;
  c.playing = 0;
  if (!c.queue.empty()) {
    c.playing = c.queue.front();
    c.queue.pop_front();
    c.cycles_left = audio_requests_[c.playing].cycles;
    audio_latch(ch, c.playing);
  } else {
    m.audio_stop(ch);
  }
  // A write on several channels completes with its last channel.
  if (!audio_queued(done)) audio_complete(done, 0);
}

void Hle::audio_flush(uint8_t mask) {
  for (int ch = 0; ch < 4; ++ch) {
    if (!(mask & (1 << ch))) continue;
    AudioChan& c = audio_[ch];
    std::vector<uint32_t> keys(c.queue.begin(), c.queue.end());
    if (c.playing) keys.push_back(c.playing);
    c.queue.clear();
    c.playing = 0;
    m.audio_stop(ch);
    for (uint32_t key : keys)
      if (!audio_queued(key)) audio_complete(key, IOERR_ABORTED);
  }
}

void Hle::audio_set(uint8_t mask, uint16_t period, uint16_t volume) {
  for (int ch = 0; ch < 4; ++ch) {
    if (!(mask & (1 << ch))) continue;
    m.audio_channel(ch).per = period;
    m.audio_channel(ch).vol = uint16_t(std::min<int>(volume, 64));
  }
}

bool Hle::audio_write(uint32_t key, uint8_t mask, uint32_t data, uint32_t length, uint16_t period, uint16_t volume,
                      uint16_t cycles, bool pervol) {
  AudioRequest& r = audio_requests_[key];
  r = {data, length, period, volume, cycles, false, 0};
  if (!data || !length || !mask) {
    audio_complete(key, 0);
    return true;
  }
  for (int ch = 0; ch < 4; ++ch) {
    if (!(mask & (1 << ch))) continue;
    AudioChan& c = audio_[ch];
    if (pervol && c.playing) {
      m.audio_channel(ch).per = period;
      m.audio_channel(ch).vol = uint16_t(std::min<int>(volume, 64));
    }
    c.queue.push_back(key);
    if (!c.playing) audio_start_next(ch);
  }
  return false;
}

bool Hle::audio_done(uint32_t key) const {
  auto it = audio_requests_.find(key);
  return it == audio_requests_.end() || it->second.done;
}

}  // namespace amiga
