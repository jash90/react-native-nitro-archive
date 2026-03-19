#pragma once

#include <string>
#include <vector>
#include <functional>

namespace archive {

struct ZipEntry {
  std::string path;
  uint64_t size;
  uint64_t compressedSize;
  bool isDirectory;
  bool isEncrypted;
};

struct ZipOptions {
  std::string password;
  int compressionLevel = 6;
};

class ZipHandler {
public:
  static void unpack(const std::string& src, const std::string& dst,
                     bool overwrite, const ZipOptions& opts,
                     const std::function<void(double, double)>& progress);

  static void pack(const std::string& src, const std::string& dst,
                   const ZipOptions& opts,
                   const std::function<void(double, double)>& progress);

  static std::vector<ZipEntry> list(const std::string& src, const ZipOptions& opts);

  static bool test(const std::string& src, const std::string& password);
};

} // namespace archive
