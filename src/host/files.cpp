// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The game's files on the host (files.hpp). Moved from the dos HLE.
#include "host/files.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace host {

std::string Files::host_path(const std::string& amiga_name, bool for_write) const {
  std::string n = amiga_name;
  auto colon = n.find(':');
  if (colon != std::string::npos) n = n.substr(colon + 1);
  auto resolve = [&](const std::string& root) -> std::string {
    fs::path p = root;
    size_t start = 0;
    while (start <= n.size()) {
      size_t slash = n.find('/', start);
      std::string comp = n.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
      start = slash == std::string::npos ? n.size() + 1 : slash + 1;
      if (comp.empty()) continue;
      fs::path next = p / comp;
      if (!fs::exists(next) && fs::is_directory(p)) {
        for (auto& e : fs::directory_iterator(p)) {
          std::string a = e.path().filename().string(), b = comp;
          std::transform(a.begin(), a.end(), a.begin(), ::tolower);
          std::transform(b.begin(), b.end(), b.begin(), ::tolower);
          if (a == b) { next = e.path(); break; }
        }
      }
      p = next;
    }
    return p.string();
  };
  std::string saved = resolve(save_dir);
  if (for_write) {
    fs::create_directories(fs::path(saved).parent_path());
    return saved;
  }
  if (fs::exists(saved)) return saved;
  return resolve(data_dir);
}

uint32_t Files::open(const std::string& name, int mode) {
  std::string path = host_path(name, mode == kModeNewFile);
  FILE* f = nullptr;
  if (mode == kModeNewFile) {
    f = fopen(path.c_str(), "w+b");
  } else if (mode == kModeOldFile) {
    if (!path.empty()) f = fopen(path.c_str(), "rb");
  } else {  // MODE_READWRITE
    std::string wp = host_path(name, true);
    if (!path.empty() && fs::exists(path) && path != wp && !fs::exists(wp)) fs::copy_file(path, wp);
    f = fopen(wp.c_str(), "r+b");
    if (!f) f = fopen(wp.c_str(), "w+b");
    path = wp;
  }
  if (!f) {
    if (verbose) std::fprintf(stderr, "Open(%s) failed (%s)\n", name.c_str(), path.c_str());
    ioerr_ = 205;
    return 0;
  }
  const uint32_t h = next_handle_++;
  files_[h] = {f, path, name, mode != kModeOldFile};
  if (verbose) std::fprintf(stderr, "Open(%s, %d) = %u\n", name.c_str(), mode, h);
  return h;
}

// MODE_OLDFILE handles are read/write on AmigaDOS. They are opened read-only
// (possibly from the data dir); the first Write copies the file into the
// save dir and reopens it there at the same position.
bool Files::make_writable(File& file) {
  if (file.writable) return true;
  const long pos = ftell(file.f);
  const std::string wp = host_path(file.name, true);
  if (!file.path.empty() && wp != file.path && fs::exists(file.path)) {
    std::error_code ec;
    fs::copy_file(file.path, wp, fs::copy_options::overwrite_existing, ec);
    if (ec) return false;
  }
  FILE* f = fopen(wp.c_str(), "r+b");
  if (!f) return false;
  fclose(file.f);
  file.f = f;
  file.path = wp;
  file.writable = true;
  fseek(f, pos, SEEK_SET);
  return true;
}

void Files::close(uint32_t handle) {
  auto it = files_.find(handle);
  if (it == files_.end()) return;
  fclose(it->second.f);
  files_.erase(it);
}

int32_t Files::read(uint32_t handle, uint8_t* to, int32_t length) {
  auto it = files_.find(handle);
  if (it == files_.end()) return -1;
  return int32_t(fread(to, 1, size_t(length), it->second.f));
}

int32_t Files::write(uint32_t handle, const uint8_t* from, int32_t length) {
  if (handle == kOutput && !files_.count(handle)) {
    fwrite(from, 1, size_t(length), stdout);
    return length;
  }
  auto it = files_.find(handle);
  if (it == files_.end()) return -1;
  if (!make_writable(it->second)) return -1;
  const size_t n = fwrite(from, 1, size_t(length), it->second.f);
  fflush(it->second.f);
  return int32_t(n);
}

int32_t Files::seek(uint32_t handle, int32_t position, int32_t mode) {
  auto it = files_.find(handle);
  if (it == files_.end()) return -1;
  const long old = ftell(it->second.f);
  const int whence = mode < 0 ? SEEK_SET : mode == 0 ? SEEK_CUR : SEEK_END;
  if (fseek(it->second.f, long(position), whence) != 0) return -1;
  return int32_t(old);
}

uint32_t Files::lock(const std::string& name) {
  const bool volume = !name.empty() && name.back() == ':';
  if (volume && disks_out) {
    ioerr_ = 225;  // ERROR_NOT_A_DOS_DISK
    return 0;
  }
  std::string p = host_path(name, false);
  if (p.empty() || !fs::exists(p)) {
    if (volume) {
      p = save_dir;  // a bare volume
    } else {
      ioerr_ = 205;
      return 0;
    }
  }
  const uint32_t h = next_handle_++;
  locks_[h] = p;
  if (verbose) std::fprintf(stderr, "Lock(%s) = %u (%s)\n", name.c_str(), h, p.c_str());
  return h;
}

void Files::unlock(uint32_t handle) { locks_.erase(handle); }

const std::string* Files::lock_path(uint32_t handle) const {
  auto it = locks_.find(handle);
  return it == locks_.end() ? nullptr : &it->second;
}

bool Files::remove(const std::string& name) {
  std::error_code ec;
  const bool ok = fs::remove(host_path(name, true), ec);
  if (!ok) ioerr_ = 205;
  return ok;
}

bool Files::rename(const std::string& from, const std::string& to) {
  std::error_code ec;
  fs::rename(host_path(from, false), host_path(to, true), ec);
  return !ec;
}

}  // namespace host
