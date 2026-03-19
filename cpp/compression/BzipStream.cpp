#include "BzipStream.hpp"
#include <stdexcept>
#include <string>

namespace archive {

BzipReader::BzipReader(FILE* fp) {
  bz_ = BZ2_bzReadOpen(&bzError_, fp, 0, 0, nullptr, 0);
  if (bzError_ != BZ_OK) {
    throw std::runtime_error("BZ2_bzReadOpen failed: " + std::to_string(bzError_));
  }
}

BzipReader::~BzipReader() {
  if (bz_) {
    BZ2_bzReadClose(&bzError_, bz_);
  }
}

int BzipReader::read(char* buf, int len) {
  if (!bz_) return 0;
  int n = BZ2_bzRead(&bzError_, bz_, buf, len);
  if (bzError_ == BZ_STREAM_END) {
    return n;
  }
  if (bzError_ != BZ_OK) {
    throw std::runtime_error("BZ2_bzRead failed: " + std::to_string(bzError_));
  }
  return n;
}

BzipWriter::BzipWriter(FILE* fp, int blockSize) {
  bz_ = BZ2_bzWriteOpen(&bzError_, fp, blockSize, 0, 30);
  if (bzError_ != BZ_OK) {
    throw std::runtime_error("BZ2_bzWriteOpen failed: " + std::to_string(bzError_));
  }
}

BzipWriter::~BzipWriter() {
  if (bz_ && !finished_) {
    // Best-effort close without throwing
    unsigned int dummy = 0;
    BZ2_bzWriteClose(&bzError_, bz_, 0, &dummy, &dummy);
  }
}

void BzipWriter::write(const char* buf, int len) {
  if (!bz_) throw std::runtime_error("BzipWriter not open");
  BZ2_bzWrite(&bzError_, bz_, const_cast<char*>(buf), len);
  if (bzError_ != BZ_OK) {
    throw std::runtime_error("BZ2_bzWrite failed: " + std::to_string(bzError_));
  }
}

void BzipWriter::finish() {
  if (!bz_ || finished_) return;
  unsigned int dummy = 0;
  BZ2_bzWriteClose(&bzError_, bz_, 0, &dummy, &dummy);
  finished_ = true;
  if (bzError_ != BZ_OK) {
    throw std::runtime_error("BZ2_bzWriteClose failed: " + std::to_string(bzError_));
  }
}

} // namespace archive
