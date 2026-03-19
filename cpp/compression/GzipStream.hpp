#pragma once

#include <zlib.h>
#include <string>

namespace archive {

class GzipReader {
public:
  explicit GzipReader(const std::string& path);
  ~GzipReader();

  GzipReader(const GzipReader&) = delete;
  GzipReader& operator=(const GzipReader&) = delete;

  // Returns bytes read, 0 on EOF
  int read(char* buf, int len);

private:
  gzFile gz_ = nullptr;
};

class GzipWriter {
public:
  explicit GzipWriter(const std::string& path, int level = 6);
  ~GzipWriter();

  GzipWriter(const GzipWriter&) = delete;
  GzipWriter& operator=(const GzipWriter&) = delete;

  void write(const char* buf, int len);
  void finish();

private:
  gzFile gz_ = nullptr;
  bool finished_ = false;
};

} // namespace archive
