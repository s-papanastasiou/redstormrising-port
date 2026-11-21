// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Audio (sound code linked into Storm, Title, RedStart and Red; RedStorm
// has none): records and variables. Offsets are a4-relative in Storm; the
// screen/sound library's data and BSS blocks shift by module (LibVar), the
// device-setup block below has a shift of its own (sound_block), a few
// variables differ irregularly (PerModule).
#pragma once

#include "game/mem/constants.hpp"
#include "game/mem/globals.hpp"

namespace game::audio {

using namespace game::mem;


inline constexpr uint16_t CMD_WRITE = 3, CMD_FLUSH = 8, ADCMD_PERVOL = 12;
inline constexpr uint8_t IOF_QUICK = 1, ADIOF_PERVOL = 16;

// A sound program: a list of words (see sound_tick).
using ProgramWord = sbe16;

// The state of a channel playing a sound program (40 bytes).
struct Channel {
  be16 duration;        // +0: ticks left of the current note; 0 idle
  sbe16 sample;         // +2
  be16 period;          // +4: 0xFFFF silent; slides by `slide`, at least kMinPeriod
  sbe16 slide;          // +6
  sbe16 volume;         // +8: 0..63
  sbe16 base_volume;    // +10: volume at the start of each note
  sbe16 env_step;       // +12: added to the volume every `env_period` ticks
  sbe16 env_counter;    // +14
  sbe16 env_period;     // +16: 0 no envelope
  sbe16 env_limit;      // +18: the envelope runs while duration <= env_limit
  be16 loop1_count;     // +20
  be16 loop2_count;     // +22
  Ptr<ProgramWord> program;  // +24
  be32 index;           // +28: next word of the program
  be32 loop1_start;     // +32
  be32 loop2_start;     // +36
};
static_assert(sizeof(Channel) == 40);

// A loaded sample (10 bytes).
struct Sample {
  be32 data;    // chip memory
  be32 length;  // bytes
  be16 loaded;
};
static_assert(sizeof(Sample) == 10);

// The engine programs: a sample, a volume and one note. The engine noise
// follows the boat's speed by rewriting the volume and period.
struct EngineProgram {
  sbe16 sample_cmd;  // kCmdSample
  sbe16 sample;
  sbe16 volume_cmd;  // kCmdVolume
  sbe16 volume;
  be16 period;
  be16 duration;
};

inline constexpr int16_t kMinPeriod = 124;    // shortest period the program player sends
inline constexpr int16_t kSilentPeriod = 124; // start_sound(kSilentPeriod, 0, ch) quiets a channel
inline constexpr int16_t kResetPeriod = 128;
inline constexpr uint16_t kRestPeriod = 0xFFFF;    // Channel::period during a rest
inline constexpr int16_t kNoEnvelopeLimit = 32767; // env_limit of a new program
inline constexpr uint16_t kSoundClockStep = 17046; // added to sound_clock every frame
// Engine noise: volume = speed / 2 + base, period = base - 24 * speed.
inline constexpr int16_t kEngineVolumeBase = 44, kEnginePeriodBase = 2152;
inline constexpr int16_t kAltEngineVolumeBase = 40, kAltEnginePeriodBase = 1400;
inline constexpr int16_t kEnginePeriodPerSpeed = 24;
inline constexpr int16_t kMaxVolume = 63;
inline constexpr uint32_t kWriteKeys = 1;       // the host audio service's keys of the channels' writes (1-4)
inline constexpr int kChannels = 4;
// The order in which every frame ticks (and silence quiets) the channels.
inline constexpr int16_t kChannelOrder[kChannels] = {0, 1, 3, 2};

// Program commands (negative words; a word >= 0 is a note: period, duration).
enum : int16_t {
  kCmdEnd = -11, kCmdVolume = -10, kCmdEnvelopeOff = -9, kCmdEnvelope = -8, kCmdSkip = -7,
  kCmdSlide = -6, kCmdSample = -5, kCmdRestart = -4, kCmdLoop2 = -3, kCmdLoop1 = -2,
  kCmdRest = -1,
};

// ---- library data (a4 offsets in Storm)

// ---- library BSS
inline constexpr LibVar<sbe16> tone_words{-12278, Block::Bss};  // set_tone's two values (words of a sound program)
inline constexpr ConstLib<sbe16> group1_samples{-12884, Block::Bss};     // [6] sample numbers
inline constexpr ConstLib<sbe16> group2_samples{-12872, Block::Bss};     // [7]
inline constexpr ConstLib<sbe16> group3_samples{-12858, Block::Bss};     // [4]
inline constexpr ConstLib<sbe16> group4_samples{-12850, Block::Bss};     // [12]
inline constexpr ConstLib<char> sounds_dir_a{-12710, Block::Bss};        // locked to find the sound file
inline constexpr ConstLib<char> sounds_file_a{-12692, Block::Bss};
inline constexpr ConstLib<char> sounds_dir_b{-12674, Block::Bss};
inline constexpr ConstLib<char> sounds_file_b{-12656, Block::Bss};
inline constexpr ConstLib<char> sounds_file_disk{-12638, Block::Bss};    // after asking for the disk
inline constexpr ConstLib<char> sounds_missing_text{-12620, Block::Bss}; // fatal error message
inline constexpr LibVar<EngineProgram> engine_program_a{-12588, Block::Bss};
inline constexpr LibVar<EngineProgram> engine_program_b{-12574, Block::Bss};
inline constexpr LibVar<EngineProgram> engine_program{-13308, Block::Bss};  // the default one

// ---- the device-setup block (its own module shift)
inline constexpr ConstPerModule<sbe16> channel_units{{-13406, 0, -27254, -20714, 6446}};  // [4] io_Unit masks

// ---- irregular per module
inline constexpr PerModule<uint8_t> ticker_server{{8974, 0, 4706, 29470, 16298}};  // interrupt node
// The vertical-blank server (an exec Interrupt node at ticker_server).
inline constexpr PerModule<be32> frame_counter{{8968, 0, 4700, 29464, 16292}};  // its address: the frame timer (the count: State)
inline constexpr uint8_t kPalTickEvery = 5;
inline constexpr int8_t kTickerPri = 0;
inline constexpr PerModule<ProgramWord> music_programs[8] = {
    {{0, 0, -27160, -20620, 6540}}, {{0, 0, -26556, -20016, 7144}},
    {{0, 0, -26382, -19842, 7318}}, {{0, 0, -26228, -19688, 7472}},
    {{0, 0, -26134, -19594, 7566}}, {{0, 0, -25604, -19064, 8096}},
    {{0, 0, -25374, -18834, 8326}}, {{0, 0, -25284, -18744, 8416}},
};

inline constexpr int16_t kEngineSampleQuiet = 16, kEngineSampleNormal = 17;

// ---- the simple sound effects: small routines of one to four
// sound_program calls, in a block at a fixed place in each module.
struct EffectStep {
  int8_t channel;
  int16_t program;  // library BSS offset of the program (Storm), or
  int8_t music;     // >= 0: music_programs[music]
};
struct Effect {
  uint16_t at;       // offset of the routine in the module's effect block (the original's)
  uint8_t cond;      // 0 always; 1 if quiet_mode is off, else `alt`; 2 only if quiet_mode is off
  uint8_t n;
  EffectStep steps[4];
  EffectStep alt;
};
inline constexpr Effect kEffects[] = {
    {0x010, 0, 4, {{3, 0, 0}, {1, 0, 1}, {0, 0, 2}, {2, 0, 3}}, {}},
    {0x044, 0, 4, {{3, 0, 4}, {1, 0, 5}, {0, 0, 6}, {2, 0, 7}}, {}},
    {0x078, 0, 3, {{3, -12228, -1}, {1, -12172, -1}, {2, -12116, -1}}, {}},
    {0x0a2, 0, 2, {{3, -12560, -1}, {2, -12560, -1}}, {}},
    {0x0c0, 0, 2, {{1, -12540, -1}, {3, -12540, -1}}, {}},
    {0x0de, 0, 2, {{3, -12526, -1}, {1, -12526, -1}}, {}},
    {0x0fc, 0, 2, {{1, -12494, -1}, {3, -12512, -1}}, {}},
    {0x212, 0, 1, {{2, -12284, -1}}, {}},
    {0x222, 0, 2, {{1, -12474, -1}, {2, -12474, -1}}, {}},
    {0x240, 0, 1, {{2, -12460, -1}}, {}},
    {0x250, 0, 1, {{1, -12442, -1}}, {}},
    {0x260, 0, 1, {{3, -12424, -1}}, {}},
    {0x270, 0, 1, {{1, -12406, -1}}, {}},
    {0x280, 0, 1, {{0, -12384, -1}}, {}},
    {0x28e, 0, 1, {{2, -12318, -1}}, {}},
    {0x29e, 1, 1, {{2, -12358, -1}}, {2, -12332, -1}},
    {0x2c4, 2, 1, {{2, -12270, -1}}, {}},
    {0x2da, 2, 1, {{2, -12256, -1}}, {}},
    {0x2f0, 2, 1, {{2, -12242, -1}}, {}},
    {0x306, 0, 1, {{3, -11726, -1}}, {}},
    {0x316, 0, 1, {{1, -11854, -1}}, {}},
    {0x326, 0, 1, {{1, -11702, -1}}, {}},
    {0x336, 0, 1, {{1, -11678, -1}}, {}},
    {0x346, 0, 1, {{2, -11868, -1}}, {}},
};
// The effect block (= sound_on) in storm, title, redstart, red.
inline constexpr const char* kSoundModules[] = {"storm", "title", "redstart", "red"};
inline constexpr uint32_t kEffectBlock[] = {0x30684, 0x224cc, 0x227c4, 0x2672c};

}  // namespace game::audio
