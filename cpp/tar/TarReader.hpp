#pragma once

#include <string>
#include <vector>
#include <functional>

namespace archive {

struct TarEntry {
  std::string path;
  size_t size = 0;
  bool isDirectory = false;
};

// Callback for each tar entry. Return false to stop iteration.
using TarEntryCallback = std::function<bool(const TarEntry& entry, const char* data)>;

// DataSource: reads up to len bytes into buf, returns actual bytes read (0 on EOF)
using DataSource = std::function<int(char* buf, int len)>;

class TarReader {
public:
  explicit TarReader(DataSource source);

  // Iterate all entries. Callback receives entry metadata + pointer to file data.
  // For directories, data is nullptr.
  void iterate(TarEntryCallback callback);

private:
  static constexpr size_t BLOCK_SIZE = 512;

  DataSource source_;

  bool readBlock(char* block);
  bool isZeroBlock(const char* block);
  TarEntry parseHeader(const char* header, std::string& longName, std::string& longLink);
  size_t parseOctal(const char* buf, int len);
  size_t alignToBlock(size_t size);
};

} // namespace archive
