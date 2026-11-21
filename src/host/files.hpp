// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The game's files on the host: AmigaDOS-like handles over the data
// directory (the WHDLoad install's data/, read-only) and the save
// directory (written files, and copies of data files on their first
// write). Names are Amiga paths ("RedStormB:red", "RedStormSave:x"): the
// volume is dropped and each component is found case-insensitively.
#pragma once

#include <cstdint>
#include <cstdio>
#include <map>
#include <string>

namespace host {

class Files {
public:
  std::string data_dir, save_dir;

  static constexpr int kModeOldFile = 1005, kModeNewFile = 1006, kModeReadWrite = 1004;
  static constexpr uint32_t kOutput = 0xFFFF;  // Output(): writes go to stdout

  // The host path of an Amiga name: the save directory's copy if there is
  // one (or for writing), else the data directory's.
  std::string host_path(const std::string& amiga_name, bool for_write) const;

  // dos Open/Close/Read/Write/Seek. Handles are nonzero; 0 / -1 on errors
  // (ioerr() says why).
  uint32_t open(const std::string& name, int mode);
  void close(uint32_t handle);
  int32_t read(uint32_t handle, uint8_t* to, int32_t length);
  int32_t write(uint32_t handle, const uint8_t* from, int32_t length);
  int32_t seek(uint32_t handle, int32_t position, int32_t mode);  // the old position
  // Lock/UnLock/DupLock: a handle naming a path.
  uint32_t lock(const std::string& name);
  void unlock(uint32_t handle);
  const std::string* lock_path(uint32_t handle) const;
  bool remove(const std::string& name);
  bool rename(const std::string& from, const std::string& to);
  uint32_t ioerr() const { return ioerr_; }
  void set_ioerr(uint32_t e) { ioerr_ = e; }
  bool verbose = false;
  // Tests: bare volumes ("RedStormA:") cannot be locked, as with the disks
  // out of the drives (the game's insert-disk requesters).
  bool disks_out = false;

private:
  struct File {
    FILE* f = nullptr;
    std::string path;
    std::string name;       // the Amiga name, for copy-on-write
    bool writable = false;  // false: opened read-only from the data or save directory
  };
  bool make_writable(File& file);
  std::map<uint32_t, File> files_;
  std::map<uint32_t, std::string> locks_;
  uint32_t next_handle_ = 1;
  uint32_t ioerr_ = 0;
};

}  // namespace host
