#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace archive {

struct SevenZipEntry {
  std::string path;
  uint64_t size;
  bool isDirectory;
};

class SevenZipHandler {
public:
  static void unpack(const std::string& src, const std::string& dst,
                     bool overwrite, const std::string& password,
                     const std::function<void(double, double)>& progress);

  static std::vector<SevenZipEntry> list(const std::string& src);

  static bool test(const std::string& src, const std::string& password);
};

} // namespace archive
