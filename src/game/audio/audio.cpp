// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Audio: the sound player shared by Storm, Title, RedStart and Red.
//
// Four Paula channels are driven through audio.device: each channel has
// three requests, one to set period and volume (ADCMD_PERVOL), one to
// flush it and one to play a sample (CMD_WRITE). Sounds are small programs
// of words (notes and commands, see sound_tick) run once per frame by
// sound_frame; effects start programs on channels; the engine noise is a
// program whose volume and period follow the boat's speed.
#include "game/audio/calls.hpp"
#include "game/audio/vars.hpp"
#include "game/common/vars.hpp"
#include "game/input/vars.hpp"
#include "game/screen/calls.hpp"
#include "game/screen/vars.hpp"
#include "game/mem/call.hpp"
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/platform/platform.hpp"
#include "game/ui/state.hpp"
#include "game/audio/state.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace game::audio {

namespace {
uint8_t unit_of(int16_t channel) { return uint8_t(channel_units[channel]); }
// The playback request of a channel (one per channel, as the original's).
uint32_t write_key(int16_t channel) { return kWriteKeys + uint32_t(channel); }
}  // namespace

// $30010: set a channel's period and volume (the period/volume request,
// sent at once).
void start_sound(int16_t period, int16_t volume, int16_t channel) {
  platform::audio_set(unit_of(channel), uint16_t(period), uint16_t(volume));
}

// $2ff7c / $2fd40: audio.device closed and opened (twelve requests, the
// device, reply ports). With the host's audio service there is nothing to
// open; close_audio returns whether `stage` is a failure, as the original.
int16_t close_audio(int16_t stage) { return stage != 0 ? 1 : 0; }
void open_audio() { state().io_result = 0; }

// $3007c: play a loaded sample on a channel: flush the channel, start the
// sample with its period and volume, and wait for the request if it has
// already completed.
void play_note(int16_t sample, int16_t period, int16_t volume, int16_t channel) {
  const Sample& s = state().samples[sample];
  if (!s.loaded) return;
  platform::audio_flush(unit_of(channel));
  platform::audio_write(write_key(channel), unit_of(channel), s.data, s.length, uint16_t(period), uint16_t(volume), 0,
                        true);
  const bool done = platform::audio_check(write_key(channel));
  state().io_result = done ? int16_t(1) : int16_t(0);
  if (done) platform::audio_wait(write_key(channel));
}

namespace {
// The program word at `addr`: from the programs the game writes (the
// engine programs, set_tone's words: audio::State), else from the
// constant data.
int16_t program_word(uint32_t addr) {
  const struct {
    uint32_t at;
    const void* bytes;
    uint32_t size;
  } written[] = {{engine_program.addr(), &state().engine_main, sizeof(EngineProgram)},
                 {engine_program_a.addr(), &state().engine_a, sizeof(EngineProgram)},
                 {engine_program_b.addr(), &state().engine_b, sizeof(EngineProgram)},
                 {tone_words.addr(), state().tones, sizeof(state().tones)}};
  for (const auto& w : written)
    if (addr >= w.at && addr + 2 <= w.at + w.size) {
      const uint8_t* b = static_cast<const uint8_t*>(w.bytes) + (addr - w.at);
      return int16_t(b[0] << 8 | b[1]);
    }
  return constant<sbe16>(addr);
}
}  // namespace

// $302b6: one frame of a channel's sound program. While a note lasts, its
// period slides and the volume envelope runs; when it ends, program words
// are read up to the next note or rest:
//   n >= 0          note: period n, then its duration
//   kCmdEnd         stop the channel
//   kCmdVolume v    base volume
//   kCmdEnvelopeOff / kCmdEnvelope step period
//   kCmdSkip        (skipped without moving the index)
//   kCmdSlide s     period slide per frame
//   kCmdSample n
//   kCmdRestart     back to the start, loops and slide cleared
//   kCmdLoop1/2 n   repeat from the loop start n times (0: set the start)
//   kCmdRest d      silence for d frames
// Words below kCmdEnd are skipped like kCmdSkip.
void sound_tick(int16_t channel) {
  Channel& ch = state().channels[channel];
  if (ch.duration == 0) {
    start_sound(kSilentPeriod, 0, channel);
    return;
  }
  ch.duration = uint16_t(ch.duration - 1);
  if (int16_t(ch.duration) > 0) {
    if (ch.period == kRestPeriod) return;  // a rest
    ch.period = uint16_t(ch.period + ch.slide);
    if (int16_t(ch.period) < kMinPeriod) ch.period = uint16_t(kMinPeriod);
    if (ch.env_limit >= int16_t(ch.duration) && ch.env_period != 0) {
      ch.env_counter = int16_t(ch.env_counter - 1);
      if (ch.env_counter <= 0) {
        ch.env_counter = ch.env_period;
        ch.volume = int16_t(ch.volume + ch.env_step);
        ch.volume = std::clamp<int16_t>(ch.volume, 0, kMaxVolume);
      }
    }
    start_sound(int16_t(ch.period), ch.volume, channel);
    return;
  }

  // The note is over: read the program from the current index. `pos`
  // follows the words read; the index only counts those that advance it.
  int32_t pos = int32_t(ch.index.get());
  auto word_at = [&](int32_t i) { return program_word(ch.program.addr + uint32_t(2 * i)); };
  auto next = [&] { return word_at(pos++); };
  for (;;) {
    const int16_t word = next();
    if (word >= 0) {
      ch.period = uint16_t(word);
      ch.duration = uint16_t(next());
      ch.volume = ch.base_volume;
      ch.index += 2;
      play_note(ch.sample, int16_t(ch.period), ch.volume, channel);
      return;
    }
    switch (word) {
      case kCmdEnd:
        ch.duration = 0;
        ch.volume = 0;
        ch.env_step = 0;
        start_sound(-1, 0, channel);
        return;
      case kCmdVolume:
        ch.index += 2;
        ch.base_volume = word_at(pos);
        ch.volume = next();
        break;
      case kCmdEnvelopeOff:
        ch.index += 1;
        ch.env_step = 0;
        ch.env_counter = 0;
        break;
      case kCmdEnvelope:
        ch.index += 3;
        ch.env_step = next();
        ch.env_period = next();
        break;
      case kCmdSlide:
        ch.index += 2;
        ch.slide = next();
        break;
      case kCmdSample:
        ch.index += 2;
        ch.sample = next();
        break;
      case kCmdRestart:
        pos = 0;
        ch.index = 0;
        ch.loop1_start = 0;
        ch.loop2_start = 0;
        ch.env_step = 0;
        ch.slide = 0;
        break;
      case kCmdLoop2:
      case kCmdLoop1: {
        be16& count = word == kCmdLoop1 ? ch.loop1_count : ch.loop2_count;
        be32& start = word == kCmdLoop1 ? ch.loop1_start : ch.loop2_start;
        const int16_t times = word_at(pos);
        if (times == 0) {  // mark the loop start
          ++pos;
          ch.index += 2;
          ch.loop1_start = ch.index;
          if (word == kCmdLoop2) ch.loop2_start = ch.index;
          break;
        }
        if (count == 0) {
          count = uint16_t(times);
          ch.index = start;
        } else {
          count = uint16_t(count - 1);
          if (count == 0) {
            ch.index += 2;
            start = ch.index;
          } else {
            ch.index = start;
          }
        }
        pos = int32_t(ch.index.get());
        break;
      }
      case kCmdRest:
        ch.period = kRestPeriod;
        ch.duration = uint16_t(next());
        ch.env_step = 0;
        ch.index += 2;
        start_sound(-1, 0, channel);
        return;
      default:  // kCmdSkip and words below kCmdEnd
        break;
    }
  }
}

// $305f0: start a sound program on a channel (read at the next tick).
void sound_program(uint32_t program, int16_t channel) {
  Channel& ch = state().channels[channel];
  ch.program = program;
  ch.index = 0;
  ch.loop1_start = 0;
  ch.loop2_start = 0;
  ch.loop1_count = 0;
  ch.loop2_count = 0;
  ch.slide = 0;
  ch.env_step = 0;
  ch.env_period = 0;
  ch.env_limit = kNoEnvelopeLimit;
  ch.duration = 1;
}

// $30282: keep the engine noise going on channel 0: start it when the
// channel is idle; while it plays, hold its note (duration 3 unless it is
// about to end).
void engine_sound() {
  Channel& ch0 = state().channels[0];
  if (ch0.duration == 0) {
    sound_program(engine_program.addr(), 0);
    return;
  }
  if (ch0.program == engine_program.addr() && ch0.duration != 1) ch0.duration = 3;
}

// $30546: run the sound player for one frame (channels 0, 1, 3, 2), with
// sound_enabled cleared meanwhile so that it does not run twice.
void sound_frame() {
  state().sound_clock += kSoundClockStep;
  if (state().sound_enabled == 0) return;
  state().sound_enabled = 0;
  if (state().engine_speed != 0 && state().engine_running == -1) engine_sound();
  for (int16_t ch : kChannelOrder) sound_tick(ch);
  state().sound_enabled = -1;
}

// $30230: the engine noise for a new speed: volume speed / 2 + 44, period
// kEnginePeriodBase - 24 * speed (faster: higher), also in the note playing now.
void set_engine(int16_t speed) {
  if (state().engine_speed == speed) return;
  state().engine_speed = speed;
  const int16_t volume = int16_t((speed >> 1) + kEngineVolumeBase);
  const uint16_t period = uint16_t(kEnginePeriodBase - kEnginePeriodPerSpeed * speed);
  state().engine_main.volume = volume;
  state().engine_main.period = period;
  Channel& ch0 = state().channels[0];
  if (ch0.program == engine_program.addr()) {
    ch0.volume = volume;
    ch0.period = period;
  }
}

// $30642: sound off and all channels quiet.
void silence() {
  state().sound_enabled = 0;
  for (int16_t ch : kChannelOrder) start_sound(kSilentPeriod, 0, ch);
}

// $2fc68 (Title $246e8, RedStart $242f8, Red $25cfc): the vertical-blank
// server: the frame counter, a tick of the music, the window's mouse
// position copied (a move is noted as an event), and on PAL an extra music
// tick every 6th frame. Returns 0 (the next server runs).
int32_t vblank_server() {
  mem::ModuleBase base(common::module_a4.addr());
  state().frame_counter = state().frame_counter + 1;
  sound_frame();
  const screen::Window& window = screen::window_at_handle(ui::state().window);
  const int16_t x = window.mouse_x, y = window.mouse_y;
  ui::Point& mouse = ui::state().mouse_pos;
  if (x != mouse.x) {
    mouse.x = x;
    ui::state().button_event = 1;
  }
  if (y != mouse.y) {
    mouse.y = y;
    ui::state().button_event = 1;
  }
  if (state().pal_ticks && (state().tick_countdown)-- == 0) {
    sound_frame();
    state().tick_countdown = kPalTickEvery;
  }
  return 0;
}
ORIGINAL(vblank_server, 0x2fc68, 0, 0x246e8, 0x242f8, 0x25cfc);

// $2fcd2 (Title $24752, RedStart $24362, Red $25d66): the vertical-blank
// server installed (named by its Interrupt node's address; the original
// filled the node with its type, priority, data (the frame counter) and
// code: the host server needs none of them).
void start_ticker() {
  const bool pal = os::pal_display();
  state().tick_countdown = kPalTickEvery;
  state().pal_ticks = pal ? 1 : 0;
  platform::add_vblank_server(ticker_server.addr(), kTickerPri, vblank_server);
  state().ticker_installed = 1;
}
ORIGINAL(start_ticker, 0x2fcd2, 0, 0x24752, 0x24362, 0x25d66);

uint32_t frames() { return state().frame_counter; }

// $2fd22: remove the module's vertical-blank interrupt server if installed.
void stop_ticker() {
  if (state().ticker_installed) platform::rem_vblank_server(ticker_server.addr());
  state().ticker_installed = 0;
}

// $30592: stop all programs and quiet all channels, leaving sound_enabled
// as it was.
void silence_all() {
  const int16_t enabled = state().sound_enabled;
  state().sound_enabled = 0;
  for (int16_t ch = 0; ch < kChannels; ++ch) state().channels[ch].duration = 0;
  for (int16_t ch : kChannelOrder) start_sound(kSilentPeriod, 0, ch);
  state().sound_enabled = enabled;
}

// $3014a: reset every channel's period and volume and flush it.
void reset_channels() {
  for (int16_t ch = 0; ch < kChannels; ++ch) {
    start_sound(kResetPeriod, 0, ch);
    platform::audio_flush(unit_of(ch));
  }
}

// $301ae: quiet mode on (channels 1-3 stopped, a quieter engine sample)
// or off. A running engine note ends at the next tick to pick it up.
void sound_mode(int16_t quiet) {
  if (state().quiet_mode == quiet) return;
  state().quiet_mode = quiet;
  if (quiet != 0) {
    for (int16_t ch = 1; ch < kChannels; ++ch) state().channels[ch].duration = 0;
    for (int16_t ch : {1, 3, 2}) start_sound(kSilentPeriod, 0, ch);
    state().engine_main.sample = kEngineSampleQuiet;
  } else {
    state().engine_main.sample = kEngineSampleNormal;
  }
  Channel& ch0 = state().channels[0];
  if (int16_t(ch0.duration) > 0 && ch0.program == engine_program.addr()) ch0.duration = 1;
}

// $30684: sound on, and channel 0 ticked at once.
void sound_on() {
  state().sound_enabled = -1;
  sound_tick(0);
}

// $3079e: the alternative engine programs for a speed: volume speed / 2 +
// 40, period 1400 - 24 * speed; also in the note playing now.
void engine_pitch(int16_t speed) {
  const int16_t volume = int16_t((speed >> 1) + kAltEngineVolumeBase);
  const uint16_t period = uint16_t(kAltEnginePeriodBase - kEnginePeriodPerSpeed * speed);
  for (EngineProgram* p : {&state().engine_a, &state().engine_b}) {
    p->volume = volume;
    p->period = period;
  }
  Channel& ch0 = state().channels[0];
  if (ch0.program == engine_program_a.addr() || ch0.program == engine_program_b.addr()) {
    if (int16_t(ch0.duration) > 0) {
      ch0.volume = volume;
      ch0.period = period;
    }
  }
}

namespace {
// $307f8 / $30836: switch channel 0 to engine program `mine`: start it if
// the channel is idle or plays the default engine; hold it if it already
// plays `mine`; end the note at once if it plays `other`.
void use_engine_program(const LibVar<EngineProgram>& mine, const LibVar<EngineProgram>& other) {
  Channel& ch0 = state().channels[0];
  if (ch0.duration == 0 || ch0.program == engine_program.addr()) {
    sound_program(mine.addr(), 0);
    return;
  }
  if (ch0.program == mine.addr()) ch0.duration = 45;
  if (ch0.program == other.addr()) ch0.duration = 1;
}
}  // namespace
void engine_program_a_on() { use_engine_program(engine_program_a, engine_program_b); }
void engine_program_b_on() { use_engine_program(engine_program_b, engine_program_a); }

namespace {
using EffectRoutine = void (*)();
EffectRoutine effect_routine(int16_t n);  // below
}  // namespace

// $309da: play effect n: 1 silence, 2 sound on, otherwise (up to 28) the
// effect routine n, with the player stopped meanwhile.
void play_effect(int16_t n) {
  if (n == 1) return silence();
  if (n == 2) return sound_on();
  if (n > 28) return;
  const int16_t enabled = state().sound_enabled;
  state().sound_enabled = 0;
  const EffectRoutine routine = effect_routine(n);
  platform::call_timed(routine);  // (the original called it through its table)
  state().sound_enabled = enabled;
}

// $30b2e: sample i's chip memory freed (if it is loaded).
void free_block(int16_t i) {
  Sample& s = state().samples[i];
  if (!s.loaded) return;
  os::free_mem(s.data, s.length);
  s.loaded = 0;
}

// $30ca2: load sample i into chip memory unless it is loaded: its length
// and data come from the preloaded block, or from the sound file (on the
// hard disk, the game disk, or after asking for it; missing: fatal).
void load_sample(int16_t i) {
  Sample& s = state().samples[i];
  if (s.loaded) return;
  const uint32_t offset = (state().sample_offsets)[i];
  const bool preloaded = state().samples_preloaded != 0;
  uint32_t file = 0;
  if (preloaded) {
    s.length = (state().preload_block).at<be32>(offset);
  } else {
    auto open_if_present = [&](const ConstLib<char>& dir, const ConstLib<char>& path) -> bool {
      const uint32_t l = os::lock(dir.str(), os::ACCESS_READ);
      if (!l) return false;
      os::unlock(l);
      file = os::open(path.str(), os::MODE_OLDFILE);
      return true;
    };
    if (!open_if_present(sounds_dir_a, sounds_file_a) && !open_if_present(sounds_dir_b, sounds_file_b)) {
      screen::need_disk();
      file = os::open(sounds_file_disk.str(), os::MODE_OLDFILE);
    }
    if (!file) screen::fatal_error(sounds_missing_text.str());
    os::seek(file, int32_t(offset + 400), os::OFFSET_BEGINNING);
    uint8_t length[4];
    os::read(file, length, 4);
    s.length = uint32_t(length[0]) << 24 | uint32_t(length[1]) << 16 | uint32_t(length[2]) << 8 | length[3];
  }
  s.data = os::alloc_mem(s.length, os::MEMF_CHIP);
  if (!s.data) {
    s.loaded = 0;
    if (!preloaded) os::close(file);
    common::c_exit(i);  // out of chip memory: exit(sample)
  }
  if (preloaded) {
    os::copy_mem((state().preload_block).addr + offset + 4, s.data, s.length);
  } else {
    os::read(file, s.data, int32_t(s.length.get()));
    os::close(file);
  }
  s.loaded = 1;
}

namespace {
// The four sample groups ($30b72 ... and $30c0a ...): load or free the
// samples listed.
template <const ConstLib<sbe16>& list, int count>
void load_group() {
  for (int16_t k = 0; k < count; ++k) load_sample(list[k]);
}
template <const ConstLib<sbe16>& list, int count>
void free_group() {
  for (int16_t k = 0; k < count; ++k) free_block(list[k]);
}
}  // namespace
void load_samples1() { load_group<group1_samples, 6>(); }
void load_samples2() { load_group<group2_samples, 7>(); }
void load_samples3() { load_group<group3_samples, 4>(); }
void load_samples4() { load_group<group4_samples, 12>(); }
void free_samples1() { free_group<group1_samples, 6>(); }
void free_samples2() { free_group<group2_samples, 7>(); }
void free_samples3() { free_group<group3_samples, 4>(); }
void free_samples4() { free_group<group4_samples, 12>(); }

// $30af6: sound off for good: stop everything, close the device if open,
// free the 20 samples.
void sound_off() {
  silence_all();
  if (state().audio_open == -1) {
    reset_channels();
    close_audio(0);
    state().audio_open = 0;
  }
  for (int16_t i = 0; i < 20; ++i) free_block(i);
}

// $30ad8: open the device if needed and enable the player.
void sound_start() {
  state().sound_enabled = 0;
  if (state().audio_open == 0) {
    open_audio();
    state().audio_open = -1;
  }
  state().sound_enabled = -1;
}

// $301a0 / $301a8: the engine noise restarts when idle / not.
void engine_on() { state().engine_running = -1; }
void engine_off() { state().engine_running = 0; }

namespace {
// The simple sound effects (kEffects).
template <int K>
void sound_effect() {
  constexpr const Effect& e = kEffects[K];
  auto run = [](const EffectStep* steps, int n) {
    for (int k = 0; k < n; ++k) {
      const EffectStep& s = steps[k];
      const uint32_t program = s.music >= 0 ? music_programs[s.music].addr()
                                            : LibVar<uint8_t>{s.program, Block::Bss}.addr();
      sound_program(program, s.channel);
    }
  };
  if (e.cond == 0 || state().quiet_mode == 0) run(e.steps, e.n);
  else if (e.cond == 1) run(&e.alt, 1);
}

// The effect routines by number (the original's table of code addresses in
// every module: the same routines in the same order).
EffectRoutine effect_routine(int16_t n) {
  static constexpr EffectRoutine kRoutines[29] = {
      silence_all,       silence,           sound_on,          sound_effect<0>,  sound_effect<1>,
      sound_effect<2>,   sound_effect<3>,   sound_effect<4>,   sound_effect<5>,  sound_effect<6>,
      sound_effect<23>,  engine_program_a_on, engine_program_b_on, sound_effect<8>, sound_effect<9>,
      sound_effect<10>,  sound_effect<11>,  sound_effect<12>,  sound_effect<13>, sound_effect<14>,
      sound_effect<16>,  sound_effect<17>,  sound_effect<18>,  sound_effect<19>, sound_effect<20>,
      sound_effect<21>,  sound_effect<22>,  sound_effect<15>,  sound_effect<7>,
  };
  if (n < 0) {
    std::fprintf(stderr, "play_effect(%d): before the effect table\n", n);
    std::abort();
  }
  return kRoutines[n];
}
}  // namespace

// Where a sound routine was in storm, title, redstart and red.
#define AUDIO_ORIGINAL(fn, storm, title, redstart, red)


// $30874: the tone of the next effect from v (higher v: shorter period,
// longer).
void set_tone(int16_t v) {
  state().tones[0] = int16_t(62 - (v >> 3));
  state().tones[1] = int16_t((v >> 1) + 200);
}
AUDIO_ORIGINAL(set_tone, 0x30874, 0x226bc, 0x229b4, 0x2691c);
AUDIO_ORIGINAL(start_sound, 0x30010, 0x21e58, 0x22150, 0x260a4);
AUDIO_ORIGINAL(close_audio, 0x2ff7c, 0x21dc4, 0x220bc, 0x26010);
AUDIO_ORIGINAL(open_audio, 0x2fd40, 0x21b88, 0x21e80, 0x25dd4);
AUDIO_ORIGINAL(play_note, 0x3007c, 0x21ec4, 0x221bc, 0x26110);
AUDIO_ORIGINAL(sound_tick, 0x302b6, 0x220fe, 0x223f6, 0x2635e);
AUDIO_ORIGINAL(sound_program, 0x305f0, 0x22438, 0x22730, 0x26698);
AUDIO_ORIGINAL(engine_sound, 0x30282, 0x220ca, 0x223c2, 0x2632a);
AUDIO_ORIGINAL(sound_frame, 0x30546, 0x2238e, 0x22686, 0x265ee);
AUDIO_ORIGINAL(set_engine, 0x30230, 0x22078, 0x22370, 0x262d8);
AUDIO_ORIGINAL(silence, 0x30642, 0x2248a, 0x22782, 0x266ea);
AUDIO_ORIGINAL(stop_ticker, 0x2fd22, 0x247a2, 0x243b2, 0x25db6);
AUDIO_ORIGINAL(silence_all, 0x30592, 0x223da, 0x226d2, 0x2663a);
AUDIO_ORIGINAL(reset_channels, 0x3014a, 0x21f92, 0x2228a, 0x261de);
AUDIO_ORIGINAL(sound_mode, 0x301ae, 0x21ff6, 0x222ee, 0x26256);
AUDIO_ORIGINAL(sound_on, 0x30684, 0x224cc, 0x227c4, 0x2672c);
AUDIO_ORIGINAL(engine_pitch, 0x3079e, 0x225e6, 0x228de, 0x26846);
AUDIO_ORIGINAL(engine_program_a_on, 0x307f8, 0x22640, 0x22938, 0x268a0);
AUDIO_ORIGINAL(engine_program_b_on, 0x30836, 0x2267e, 0x22976, 0x268de);
AUDIO_ORIGINAL(play_effect, 0x309da, 0x22822, 0x22b1a, 0x26a82);
AUDIO_ORIGINAL(load_sample, 0x30ca2, 0x22aea, 0x22de2, 0x26d4a);
AUDIO_ORIGINAL(load_samples1, 0x30b72, 0x229ba, 0x22cb2, 0x26c1a);
AUDIO_ORIGINAL(load_samples2, 0x30b98, 0x229e0, 0x22cd8, 0x26c40);
AUDIO_ORIGINAL(load_samples3, 0x30bbe, 0x22a06, 0x22cfe, 0x26c66);
AUDIO_ORIGINAL(load_samples4, 0x30be4, 0x22a2c, 0x22d24, 0x26c8c);
AUDIO_ORIGINAL(free_block, 0x30b2e, 0x22976, 0x22c6e, 0x26bd6);
AUDIO_ORIGINAL(free_samples1, 0x30c0a, 0x22a52, 0x22d4a, 0x26cb2);
AUDIO_ORIGINAL(free_samples2, 0x30c30, 0x22a78, 0x22d70, 0x26cd8);
AUDIO_ORIGINAL(free_samples3, 0x30c56, 0x22a9e, 0x22d96, 0x26cfe);
AUDIO_ORIGINAL(free_samples4, 0x30c7c, 0x22ac4, 0x22dbc, 0x26d24);
AUDIO_ORIGINAL(sound_off, 0x30af6, 0x2293e, 0x22c36, 0x26b9e);
AUDIO_ORIGINAL(sound_start, 0x30ad8, 0x22920, 0x22c18, 0x26b80);
AUDIO_ORIGINAL(engine_on, 0x301a0, 0x21fe8, 0x222e0, 0x26248);
AUDIO_ORIGINAL(engine_off, 0x301a8, 0x21ff0, 0x222e8, 0x26250);

}  // namespace game::audio
