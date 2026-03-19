#pragma once

#include <string>
#include <functional>

namespace archive {

// DataSink: writes len bytes from buf
using DataSink = std::function<void(const char* buf, int len)>;

class TarWriter {
public:
  explicit TarWriter(DataSink sink);

  // Write a file entry. localPath is the path on disk, archiveName is the name in the archive.
  void writeFileEntry(const std::string& archiveName, const std::string& localPath);

  // Write a directory entry
  void writeDirEntry(const std::string& archiveName);

  // Write end-of-archive marker (two zero blocks)
  void finish();

private:
  static constexpr size_t BLOCK_SIZE = 512;

  DataSink sink_;

  void writeHeader(const std::string& name, size_t size, bool isDir);
  void writeLongName(const std::string& name);
  void writePadding(size_t dataSize);
  void writeOctal(char* buf, int offset, int len, unsigned long value);
  void computeChecksum(char* header);
};

} // namespace archive
