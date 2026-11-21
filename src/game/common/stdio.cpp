// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The C runtime's exit through stdio (all five modules): exit() writes out
// what the buffered files hold, _exit closes the open file handles (Ctrl-C
// is checked at each), then the start-up's exit.
#include "game/common/strings.hpp"
#include "game/common/vars.hpp"
#include "game/mem/call.hpp"
#include "game/mem/os.hpp"
#include "game/mem/register.hpp"
#include "game/screen/vars.hpp"
#include "game/ui/state.hpp"
#include "game/common/state.hpp"

namespace game::common {

void c_exit_files(int16_t code);

// $34358 (and copies): on Ctrl-C/D the break handler would be asked
// whether the program ends. No module sets one (break_handler stays 0),
// so only the signals are taken.
void check_break() { os::set_signal(0, kStartSignals); }
ORIGINAL(check_break, 0x34358, 0x23afc, 0x25644, 0x28860, 0x2f070);

// $33928 (and copies): a file handle closed (Ctrl-C checked first if there
// is a break handler). Returns 0.
int16_t close_handle(uint32_t handle) {
  if (state().break_handler) check_break();
  os::close(handle);
  return 0;
}
ORIGINAL(close_handle, 0x33928, 0x23208, 0x24d50, 0x27ff0, 0x2e800);

// $34178 (and copies): _exit: the open file handles closed, last first.
void c_exit_files(int16_t code) {
  for (int16_t i = int16_t(state().fd_count - 1); i >= 0; --i) {
    const FdEntry& e = fd_table[i];
    if ((e.flags & 0xff) && !(e.flags & kFdNoClose)) close_handle(e.handle);
  }
  program_exit(code);
}
ORIGINAL(c_exit_files, 0x34178, 0x23938, 0x25480, 0x287d0, 0x2efe0);

// $33b04 (and copies): the table entry of file descriptor `fd` (0, errno
// EBADF, when it is not open).
uint32_t fd_entry(int16_t fd) {
  state().dos_error = 0;
  if (fd >= 0 && fd < state().fd_count && fd_table[fd].flags) return fd_table.addr() + uint32_t(fd * int16_t(sizeof(FdEntry)));
  state().c_errno = kEbadf;
  return 0;
}
ORIGINAL(fd_entry, 0x33b04, 0x233d8, 0x24f20, 0x281cc, 0x2e9dc);

// $337f4 (and copies): dos Write (Ctrl-C checked first); on failure the
// dos error and errno EIO. Returns what Write did.
int32_t dos_write(uint32_t handle, uint32_t buffer, int16_t n) {
  if (state().break_handler) check_break();
  state().dos_error = 0;
  const int32_t written = os::write(handle, buffer, n);
  if (written == -1) {
    state().dos_error = int16_t(os::io_err());
    state().c_errno = kEio;
  }
  return written;
}
ORIGINAL(dos_write, 0x337f4, 0x230bc, 0x24c04, 0x27ebc, 0x2e6cc);

// $33844 (and copies): dos Seek with lseek's mode (Ctrl-C checked first);
// on failure the dos error and errno EINVAL. Returns the new position
// (any other mode: the mode).
int32_t dos_seek(uint32_t handle, int32_t position, int16_t mode) {
  if (state().break_handler) check_break();
  state().dos_error = 0;
  const int32_t old = os::seek(handle, position, int32_t(uint16_t(mode)) - 1);
  if (old == -1) {
    state().dos_error = int16_t(os::io_err());
    state().c_errno = kEinval;
  }
  switch (mode) {
    case kSeekSet: return position;
    case kSeekCur: return old + position;
    case kSeekEnd: return os::seek(handle, 0, os::OFFSET_CURRENT);
    default: return mode;
  }
}
ORIGINAL(dos_seek, 0x33844, 0x23124, 0x24c6c, 0x27f0c, 0x2e71c);

// $3364c (and copies): lseek(fd, position, mode). -1: failed.
int32_t c_lseek(int16_t fd, int32_t position, int16_t mode) {
  const uint32_t e = fd_entry(fd);
  if (!e) return -1;
  const int32_t at = dos_seek(constant<FdEntry>(e).handle, position, mode);
  return state().dos_error ? -1 : at;
}
ORIGINAL(c_lseek, 0x3364c, 0x22f30, 0x24a50, 0x27e1c, 0x2e62c);

// $33cfc (and copies): write(fd, buffer, n), at the end of the file when
// it was opened for appending. -1: failed.
int32_t c_write(int16_t fd, uint32_t buffer, int16_t n) {
  const uint32_t e = fd_entry(fd);
  if (!e) return -1;
  if (constant<FdEntry>(e).flags & kFdAppend) c_lseek(fd, 0, kSeekEnd);
  const int32_t written = dos_write(constant<FdEntry>(e).handle, buffer, n);
  return state().dos_error ? -1 : written;
}
ORIGINAL(c_write, 0x33cfc, 0x234c4, 0x2500c, 0x28354, 0x2eb64);

// $3379c (and copies): exit: each buffered file being written gives up
// what its buffer holds, then _exit.
void c_exit(int16_t code) {
  for (uint32_t f = stdio_files.addr(); f; f = constant<CFile>(f).next) {
    const CFile& file = constant<CFile>(f);
    if ((file.flags & kFileUnbuffered) || !(file.flags & kFileWriting)) continue;
    const int16_t n = int16_t(file.ptr - file.base);
    if (n) c_write(file.fd, file.base, n);
  }
  c_exit_files(code);
}
ORIGINAL(c_exit, 0x3379c, 0x23064, 0x24bac, 0x27e64, 0x2e674);

}  // namespace game::common
