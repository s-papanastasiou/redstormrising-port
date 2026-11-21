// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// The game's files from the WHDLoad install archive or the v843.02 disks
// (game_archive.hpp): an LHA reader (-lh5- and -lh0-, level-1 headers,
// CRC-checked), an ADF reader (AmigaDOS OFS and FFS) and the files the
// game uses, checked against the build this port was made for.
#include "host/game_archive.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>

namespace host {

namespace {

// ---- what the game uses -----------------------------------------------------

struct Wanted {
  const char* name;  // in the archive's data/ directory
  uint32_t size;
  uint64_t fnv;      // FNV-1a 64 of the file
};
const Wanted kWanted[] = {
    {"ATBMS", 36954, 0x7871aee17bebe27full},     {"ATSCR", 26232, 0x9cd5cf09dc1f7a98ull},
    {"BMS_index", 400, 0x772f02f8bffa9d7dull},   {"opening", 18548, 0x9d4c487d3bf6523eull},
    {"osg", 47408, 0xd02054a0415a26a0ull},       {"osg_index", 400, 0x1c6de8b8e001c74aull},
    {"pics", 111222, 0xb500beaa5fc0e9a2ull},     {"pics_index", 400, 0x2f471597c7beccdeull},
    {"ships", 47508, 0x93e7029879755e74ull},     {"ships_index", 400, 0xd65a9786bf84f16full},
    {"sounds", 97680, 0x1b3ca2523fe4ee11ull},    {"TITLEBM", 35832, 0xccb77368e6448dc8ull},
    {"titlescr", 1304, 0x5470eb49e055919eull},   {"RedStorm", 29948, 0x992e4a15903098feull},
    {"Title", 41948, 0x11f108e99e8aff00ull},      {"RedStart", 83512, 0x795d8561e3d46cd5ull},
    {"Red", 125232, 0xf730988d32eec82bull},       {"Storm", 111208, 0x084e6a5d4cb2d5dfull},
    {"RedStorm1.fnt", 848, 0x5ecf930861aa3bbdull}, {"RedStorm2.fnt", 1532, 0x9de89923081dec2aull},
    {"RedStorm3.fnt", 868, 0x0fcd85384ae5aa44ull},
};

uint64_t fnv1a(const std::vector<uint8_t>& d) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (uint8_t b : d) h = (h ^ b) * 0x100000001b3ull;
  return h;
}

// ---- LHA --------------------------------------------------------------------

uint16_t crc16(const std::vector<uint8_t>& d) {  // CRC-16/ARC, as LHA's
  uint16_t crc = 0;
  for (uint8_t b : d) {
    crc ^= b;
    for (int k = 0; k < 8; ++k) crc = (crc & 1) ? uint16_t((crc >> 1) ^ 0xA001) : uint16_t(crc >> 1);
  }
  return crc;
}

// The -lh5- decoder: blocks of static Huffman codes (8 KB window).
class Lh5 {
public:
  Lh5(const uint8_t* in, size_t n) : in_(in), n_(n) {}

  bool decode(std::vector<uint8_t>& out, size_t size) {
    std::vector<uint8_t> window(kWindow, ' ');
    size_t wpos = 0;
    out.clear();
    out.reserve(size);
    uint32_t block_left = 0;
    while (out.size() < size) {
      if (block_left == 0) {
        block_left = bits(16);
        if (!read_pt(kNt, kTbit, 3) || !read_c() || !read_pt(kNp, kPbit, -1)) return false;
        p_ = pt_;
      }
      --block_left;
      const int c = c_.decode(*this);
      if (c < 0) return false;
      if (c < 256) {
        out.push_back(uint8_t(c));
        window[wpos++ & (kWindow - 1)] = uint8_t(c);
      } else {
        const int length = c - 256 + 3;
        int p = p_.decode(*this);
        if (p < 0) return false;
        if (p > 1) p = (1 << (p - 1)) + int(bits(p - 1));
        size_t from = wpos - size_t(p) - 1;
        for (int k = 0; k < length && out.size() < size; ++k) {
          const uint8_t b = window[from++ & (kWindow - 1)];
          out.push_back(b);
          window[wpos++ & (kWindow - 1)] = b;
        }
      }
    }
    return !error_;
  }

private:
  static constexpr size_t kWindow = 8192;
  static constexpr int kNc = 510, kNt = 19, kTbit = 5, kNp = 14, kPbit = 4, kCbit = 9;

  // Canonical Huffman codes from their lengths (MSB first, shorter first,
  // then by symbol); a code of one symbol takes no bits.
  struct Code {
    std::vector<int> first, count, symbols;
    int single = -1;
    void build(const std::vector<uint8_t>& len) {
      first.assign(17, 0);
      count.assign(17, 0);
      symbols.clear();
      single = -1;
      for (uint8_t l : len)
        if (l) ++count[l];
      int code = 0;
      for (int l = 1; l <= 16; ++l) {
        first[l] = code;
        code = (code + count[l]) << 1;
      }
      for (int l = 1; l <= 16; ++l)
        for (size_t s = 0; s < len.size(); ++s)
          if (len[s] == l) symbols.push_back(int(s));
    }
    int decode(Lh5& d) const {
      if (single >= 0) return single;
      int code = 0, index = 0;
      for (int l = 1; l <= 16; ++l) {
        code = (code << 1) | int(d.bits(1));
        if (code - first[l] < count[l]) return symbols[size_t(index + code - first[l])];
        index += count[l];
      }
      return -1;
    }
  };

  uint32_t bits(int n) {
    uint32_t v = 0;
    for (int k = 0; k < n; ++k) {
      const size_t byte = bitpos_ >> 3;
      uint32_t bit = 0;
      if (byte < n_) bit = (in_[byte] >> (7 - (bitpos_ & 7))) & 1;
      else error_ = true;
      ++bitpos_;
      v = (v << 1) | bit;
    }
    return v;
  }

  bool read_pt(int nn, int nbit, int special) {
    std::vector<uint8_t> len(size_t(nn), 0);
    const int n = int(bits(nbit));
    if (n == 0) {
      pt_.build(len);
      pt_.single = int(bits(nbit));
      return true;
    }
    if (n > nn) return false;
    int i = 0;
    while (i < n) {
      int c = int(bits(3));
      if (c == 7)
        while (bits(1)) ++c;
      len[size_t(i++)] = uint8_t(c);
      if (i == special)
        for (int z = int(bits(2)); z > 0 && i < nn; --z) len[size_t(i++)] = 0;
    }
    pt_.build(len);
    return true;
  }

  bool read_c() {
    std::vector<uint8_t> len(kNc, 0);
    const int n = int(bits(kCbit));
    if (n == 0) {
      c_.build(len);
      c_.single = int(bits(kCbit));
      return true;
    }
    if (n > kNc) return false;
    int i = 0;
    while (i < n) {
      int c = pt_.decode(*this);
      if (c < 0) return false;
      if (c <= 2) {
        int zeros = c == 0 ? 1 : c == 1 ? int(bits(4)) + 3 : int(bits(kCbit)) + 20;
        while (zeros-- > 0 && i < kNc) len[size_t(i++)] = 0;
      } else {
        len[size_t(i++)] = uint8_t(c - 2);
      }
    }
    c_.build(len);
    return true;
  }

  const uint8_t* in_;
  size_t n_;
  size_t bitpos_ = 0;
  bool error_ = false;
  Code pt_, c_, p_;
};

struct Entry {
  std::string name;  // with its directory, '/'-separated
  std::vector<uint8_t> data;
};

bool read_lha(const std::vector<uint8_t>& d, std::vector<Entry>& entries, std::string& err) {
  size_t p = 0;
  auto u16 = [&](size_t at) { return uint32_t(d[at]) | uint32_t(d[at + 1]) << 8; };
  auto u32 = [&](size_t at) { return u16(at) | u16(at + 2) << 16; };
  while (p + 22 < d.size() && d[p] != 0) {
    const uint32_t header = d[p];
    const std::string method(reinterpret_cast<const char*>(&d[p + 2]), 5);
    uint32_t packed = u32(p + 7);
    const uint32_t size = u32(p + 11);
    const int level = d[p + 20];
    if (level != 1) {
      err = "LHA header level " + std::to_string(level) + " (only level 1 is read)";
      return false;
    }
    const uint32_t name_length = d[p + 21];
    std::string name(reinterpret_cast<const char*>(&d[p + 22]), name_length);
    const uint32_t crc = u16(p + 22 + name_length);
    size_t q = p + header + 2;  // after the base header: the first extended header
    uint32_t next = u16(q - 2);
    std::string dir;
    while (next) {  // extended headers (counted in `packed` in level 1)
      if (d[q] == 2) {
        dir.assign(reinterpret_cast<const char*>(&d[q + 1]), next - 3);
        for (char& c : dir)
          if (uint8_t(c) == 0xFF) c = '/';
      }
      packed -= next;
      q += next;
      next = u16(q - 2);
    }
    Entry e{dir + name, {}};
    if (method == "-lh0-") {
      e.data.assign(d.begin() + long(q), d.begin() + long(q + size));
    } else if (method == "-lh5-") {
      Lh5 lh5(&d[q], packed);
      if (!lh5.decode(e.data, size)) {
        err = e.name + ": broken -lh5- data";
        return false;
      }
    } else if (method != "-lhd-") {
      err = e.name + ": compression " + method + " not supported";
      return false;
    }
    if (method != "-lhd-" && crc16(e.data) != crc) {
      err = e.name + ": CRC mismatch";
      return false;
    }
    entries.push_back(std::move(e));
    p = q + packed;
  }
  return true;
}

// ---- ADF --------------------------------------------------------------------

// The files in an AmigaDOS disk image's root directory (OFS or FFS).
bool read_adf(const std::vector<uint8_t>& d, std::vector<Entry>& entries, std::string& err) {
  constexpr size_t kBlock = 512;
  const size_t blocks = d.size() / kBlock;
  const bool ffs = d[3] & 1;
  auto l = [&](size_t block, int i) {  // long i of a block (negative: from its end)
    const size_t at = block * kBlock + size_t(i < 0 ? int(kBlock / 4) + i : i) * 4;
    return uint32_t(d[at]) << 24 | uint32_t(d[at + 1]) << 16 | uint32_t(d[at + 2]) << 8 | d[at + 3];
  };
  auto valid = [&](uint32_t b) { return b >= 2 && b < blocks; };
  const size_t root = blocks / 2;
  for (int slot = 6; slot < 6 + 72; ++slot) {  // the root's hash table
    int guard = 0;
    for (uint32_t h = l(root, slot); h; h = l(h, -4)) {  // each hash chain
      if (!valid(h) || ++guard > int(blocks)) {
        err = "broken directory";
        return false;
      }
      if (int32_t(l(h, -1)) != -3) continue;  // a file (ST_FILE); directories skipped
      const size_t name_at = h * kBlock + kBlock - 80;
      Entry e{std::string(reinterpret_cast<const char*>(&d[name_at + 1]), std::min<size_t>(d[name_at], 30)), {}};
      const uint32_t size = l(h, -47);
      if (ffs) {  // the data blocks listed in the header and its extension blocks
        for (uint32_t list = h; list && e.data.size() < size; list = l(list, -2)) {
          if (!valid(list)) break;
          for (int k = 0; k < int(l(list, 2)) && k < 72 && e.data.size() < size; ++k) {
            const uint32_t b = l(list, 6 + 71 - k);
            if (!valid(b)) break;
            e.data.insert(e.data.end(), d.begin() + long(b * kBlock), d.begin() + long(b * kBlock + kBlock));
          }
        }
      } else {  // OFS: a chain of data blocks with 24-byte headers
        for (uint32_t b = l(h, 4); b && e.data.size() < size; b = l(b, 4)) {
          if (!valid(b) || l(b, 3) > kBlock - 24) break;
          e.data.insert(e.data.end(), d.begin() + long(b * kBlock + 24), d.begin() + long(b * kBlock + 24 + l(b, 3)));
        }
      }
      if (e.data.size() < size) {
        err = e.name + ": broken file";
        return false;
      }
      e.data.resize(size);
      entries.push_back(std::move(e));
    }
  }
  return true;
}

// The crack on the v843.02 disks: RedStart's manual question skipped by a
// NOP in place of the check's branch (file offset $2f8, hunk offset $2d4).
// The original's beq put back, so the file is the WHDLoad install's (whose
// own patch is applied when it is loaded, see --protection).
void uncrack(Entry& e) {
  constexpr size_t kAt = 0x2f8;
  if (e.data.size() == 83512 && e.data[kAt] == 0x4e && e.data[kAt + 1] == 0x71) {
    e.data[kAt] = 0x67;
    e.data[kAt + 1] = 0x68;
  }
}

std::string lower(std::string s) {
  for (char& c : s) c = char(std::tolower(uint8_t(c)));
  return s;
}

}  // namespace

bool has_game_files(const std::string& dir) {
  namespace fs = std::filesystem;
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) return false;
  std::map<std::string, uintmax_t> sizes;
  for (const auto& e : fs::directory_iterator(dir, ec)) sizes[lower(e.path().filename().string())] = e.file_size(ec);
  for (const Wanted& w : kWanted) {
    auto it = sizes.find(lower(w.name));
    if (it == sizes.end() || it->second != w.size) return false;
  }
  return true;
}

bool extract_game_files(const std::vector<std::string>& sources, const std::string& out_dir, std::string& err) {
  std::vector<Entry> entries;
  std::map<std::string, const Entry*> by_name;  // lower-case file name
  std::string from;                               // the sources, for messages
  for (const std::string& path : sources) {
    from += (from.empty() ? "" : " + ") + path;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      err = path + ": cannot read";
      return false;
    }
    const std::vector<uint8_t> image((std::istreambuf_iterator<char>(in)), {});
    std::vector<Entry> found;
    const bool adf = image.size() >= 2 * 512 && image.size() % 512 == 0 && image[0] == 'D' && image[1] == 'O' &&
                     image[2] == 'S' && image[3] < 8;
    if (!(adf ? read_adf(image, found, err) : read_lha(image, found, err))) {
      err = path + ": " + err;
      return false;
    }
    if (found.empty()) {
      err = path + (adf ? ": no files on the disk" : ": neither an LHA archive nor an ADF disk image");
      return false;
    }
    for (Entry& e : found) {
      if (adf) uncrack(e);
      entries.push_back(std::move(e));
    }
  }
  for (const Entry& e : entries) {
    const size_t at = e.name.rfind("data/");  // the archive's data/ directory; a disk's root
    if (at != std::string::npos) by_name[lower(e.name.substr(at + 5))] = &e;
    else if (e.name.find('/') == std::string::npos) by_name.emplace(lower(e.name), &e);
  }
  // Every file checked before any is written.
  for (const Wanted& w : kWanted) {
    auto it = by_name.find(lower(w.name));
    if (it == by_name.end()) {
      err = from + ": " + w.name + " is not there (both disks of v843.02 are needed, or the WHDLoad install)";
      return false;
    }
    if (it->second->data.size() != w.size || fnv1a(it->second->data) != w.fnv) {
      err = from + ": " + w.name + " is another build's (the port runs v843.02 / the WHDLoad install)";
      return false;
    }
  }
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::create_directories(out_dir, ec);
  for (const Wanted& w : kWanted) {
    const std::vector<uint8_t>& data = by_name[lower(w.name)]->data;
    std::ofstream f(fs::path(out_dir) / w.name, std::ios::binary);
    f.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
    if (!f) {
      err = (fs::path(out_dir) / w.name).string() + ": cannot write";
      return false;
    }
  }
  return true;
}

}  // namespace host
