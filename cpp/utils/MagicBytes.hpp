#pragma once

#include <string>
#include <cstdio>
#include <cstring>
#include <cstdint>

namespace archive {

enum class ArchiveFormat {
  Tar,
  Gz,
  Bz2,
  TarGz,
  TarBz2,
  Zip,
  SevenZ,
  Unknown
};

inline std::string formatToString(ArchiveFormat fmt) {
  switch (fmt) {
    case ArchiveFormat::Tar: return "tar";
    case ArchiveFormat::Gz: return "gz";
    case ArchiveFormat::Bz2: return "bz2";
    case ArchiveFormat::TarGz: return "tar.gz";
    case ArchiveFormat::TarBz2: return "tar.bz2";
    case ArchiveFormat::Zip: return "zip";
    case ArchiveFormat::SevenZ: return "sevenz";
    default: return "unknown";
  }
}

inline ArchiveFormat stringToFormat(const std::string& s) {
  if (s == "tar") return ArchiveFormat::Tar;
  if (s == "gz") return ArchiveFormat::Gz;
  if (s == "bz2") return ArchiveFormat::Bz2;
  if (s == "tar.gz") return ArchiveFormat::TarGz;
  if (s == "tar.bz2") return ArchiveFormat::TarBz2;
  if (s == "zip") return ArchiveFormat::Zip;
  if (s == "sevenz" || s == "7z") return ArchiveFormat::SevenZ;
  return ArchiveFormat::Unknown;
}

inline ArchiveFormat detectFormat(const std::string& path) {
  FILE* fp = std::fopen(path.c_str(), "rb");
  if (!fp) return ArchiveFormat::Unknown;

  uint8_t header[262];
  std::memset(header, 0, sizeof(header));
  size_t bytesRead = std::fread(header, 1, sizeof(header), fp);
  std::fclose(fp);

  if (bytesRead < 3) return ArchiveFormat::Unknown;

  // Check for ZIP: starts with PK\x03\x04
  if (bytesRead >= 4 &&
      header[0] == 0x50 && header[1] == 0x4B &&
      header[2] == 0x03 && header[3] == 0x04) {
    return ArchiveFormat::Zip;
  }

  // Check for 7z: starts with 37 7A BC AF 27 1C
  if (bytesRead >= 6 &&
      header[0] == 0x37 && header[1] == 0x7A &&
      header[2] == 0xBC && header[3] == 0xAF &&
      header[4] == 0x27 && header[5] == 0x1C) {
    return ArchiveFormat::SevenZ;
  }

  // Check for bzip2: starts with "BZh"
  bool isBz2 = (header[0] == 'B' && header[1] == 'Z' && header[2] == 'h');

  // Check for gzip: starts with 1F 8B
  bool isGz = (header[0] == 0x1F && header[1] == 0x8B);

  // Check for tar: "ustar" at offset 257
  bool isTar = false;
  if (bytesRead >= 262) {
    isTar = (std::memcmp(header + 257, "ustar", 5) == 0);
  }

  if (isTar) return ArchiveFormat::Tar;

  // For compressed formats, use file extension to distinguish tar.* from plain *
  if (isBz2) {
    // Check extension
    auto dotPos = path.rfind('.');
    if (dotPos != std::string::npos) {
      auto ext = path.substr(dotPos);
      if (ext == ".bz2") {
        // Check if there's tar before .bz2
        auto prevDot = path.rfind('.', dotPos - 1);
        if (prevDot != std::string::npos) {
          auto prevExt = path.substr(prevDot, dotPos - prevDot);
          if (prevExt == ".tar") return ArchiveFormat::TarBz2;
        }
        // Also check common pattern: .tbz2
        if (ext == ".tbz2" || ext == ".tbz") return ArchiveFormat::TarBz2;
        return ArchiveFormat::TarBz2; // Default to tar.bz2 for .bz2 files (most common use case)
      }
    }
    return ArchiveFormat::TarBz2; // Default
  }

  if (isGz) {
    auto dotPos = path.rfind('.');
    if (dotPos != std::string::npos) {
      auto ext = path.substr(dotPos);
      if (ext == ".gz") {
        auto prevDot = path.rfind('.', dotPos - 1);
        if (prevDot != std::string::npos) {
          auto prevExt = path.substr(prevDot, dotPos - prevDot);
          if (prevExt == ".tar") return ArchiveFormat::TarGz;
        }
        // .tgz
        if (ext == ".tgz") return ArchiveFormat::TarGz;
        return ArchiveFormat::TarGz; // Default to tar.gz for .gz files
      }
    }
    return ArchiveFormat::TarGz; // Default
  }

  return ArchiveFormat::Unknown;
}

} // namespace archive
