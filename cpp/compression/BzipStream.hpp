#pragma once

#include <cstdio>

// Use vendored bzip2
#include "../vendor/bzip2/bzlib.h"

namespace archive {

class BzipReader {
public:
  explicit BzipReader(FILE* fp);
  ~BzipReader();

  BzipReader(const BzipReader&) = delete;
  BzipReader& operator=(const BzipReader&) = delete;

  // Returns bytes read, 0 on EOF
  int read(char* buf, int len);

private:
  BZFILE* bz_ = nullptr;
  int bzError_ = BZ_OK;
};

class BzipWriter {
public:
  explicit BzipWriter(FILE* fp, int blockSize = 9);
  ~BzipWriter();

  BzipWriter(const BzipWriter&) = delete;
  BzipWriter& operator=(const BzipWriter&) = delete;

  void write(const char* buf, int len);
  void finish();

private:
  BZFILE* bz_ = nullptr;
  int bzError_ = BZ_OK;
  bool finished_ = false;
};

} // namespace archive
