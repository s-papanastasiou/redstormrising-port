// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Devices of the C runtime: the keyboard device opened and closed, the
// system time from timer.device and the random seed taken from it; Red's
// screen to the front.
#include "game/audio/calls.hpp"
#include "game/common/strings.hpp"
#include "game/common/vars.hpp"
#include "game/mem/globals.hpp"
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/platform/platform.hpp"
#include "game/screen/vars.hpp"
#include "game/ui/state.hpp"
#include "game/common/state.hpp"
#include "game/common/memory.hpp"
#include "game/common/process.hpp"
#include "game/mem/state.hpp"

#include <cstdio>
#include <cstdlib>

namespace game::common {

// keyboard.device opened and closed (OpenDevice's and CloseDevice's
// time; the keys come from the key matrix). The original made a message
// port and an I/O request for it ($33a74 create_port, $2330c
// create_std_io, $338e8 delete_port, $2319c delete_std_io), which nothing
// read.
ORIGINAL_INSIDE(create_port, 0x33a74, 0x23348, 0x24e90, 0x2813c, 0x2e94c);
ORIGINAL_INSIDE(delete_port, 0x338e8, 0x231c8, 0x24d10, 0x27fb0, 0x2e7c0);
ORIGINAL_INSIDE(create_ext_io, 0x33cb4, 0, 0, 0x2830c, 0x2eb1c);
ORIGINAL_INSIDE(create_std_io, 0, 0x2330c, 0, 0, 0);
ORIGINAL_INSIDE(delete_ext_io, 0x33c84, 0, 0, 0x282dc, 0x2eaec);
ORIGINAL_INSIDE(delete_std_io, 0, 0x2319c, 0, 0, 0);
void open_keyboard() { os::open_device_time(); }
void close_keyboard() { os::close_device_time(); }

// $33b50 (RedStart $28218, Red $2ea28): the system time as two words
// (the low words of the seconds and microseconds). Returns 0. (The
// original asked timer.device through a port and a request.)
int16_t get_system_time(uint16_t out[2]) {
  uint32_t seconds, micros;
  platform::system_time(seconds, micros);
  out[0] = uint16_t(seconds);
  out[1] = uint16_t(micros);
  return 0;
}
ORIGINAL_INSIDE(get_system_time, 0x33b50, 0, 0, 0x28218, 0x2ea28);

// $33340 (RedStart $269b0, Red $2a2b0): the random generator seeded from
// the microseconds of the system time.
void seed_random() {
  uint16_t time[2];
  get_system_time(time);
  state().random_seed = uint32_t(int32_t(int16_t(time[1])));
}
ORIGINAL(seed_random, 0x33340, 0, 0, 0x269b0, 0x2a2b0);

// $28244 (Red): its screen to the front, its window active.
void screen_to_front() {
  os::screen_to_front((ui::state().screen).addr);
  os::activate_window(ui::state().window);
}
ORIGINAL(screen_to_front, 0, 0, 0, 0, 0x28244);

}  // namespace game::common
