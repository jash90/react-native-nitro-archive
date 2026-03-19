#include "GzipStream.hpp"

namespace archive {

GzipReader::GzipReader(const std::string& path) {
  gz_ = gzopen(path.c_str(), "rb");
  if (!gz_) {
    throw std::runtime_error("gzopen failed: " + path);
  }
}

GzipReader::~GzipReader() {
  if (gz_) gzclose(gz_);
}

int GzipReader::read(char* buf, int len) {
  if (!gz_) return 0;
  int n = gzread(gz_, buf, static_cast<unsigned>(len));
  if (n < 0) {
    int errnum;
    const char* msg = gzerror(gz_, &errnum);
    throw std::runtime_error(std::string("gzread failed: ") + (msg ? msg : "unknown"));
  }
  return n;
}

GzipWriter::GzipWriter(const std::string& path, int level) {
  std::string mode = "wb" + std::to_string(level);
  gz_ = gzopen(path.c_str(), mode.c_str());
  if (!gz_) {
    throw std::runtime_error("gzopen for writing failed: " + path);
  }
}

GzipWriter::~GzipWriter() {
  if (gz_ && !finished_) {
    gzclose(gz_);
  }
}

void GzipWriter::write(const char* buf, int len) {
  if (!gz_) throw std::runtime_error("GzipWriter not open");
  int written = gzwrite(gz_, buf, static_cast<unsigned>(len));
  if (written <= 0) {
    throw std::runtime_error("gzwrite failed");
  }
}

void GzipWriter::finish() {
  if (!gz_ || finished_) return;
  finished_ = true;
  if (gzclose(gz_) != Z_OK) {
    gz_ = nullptr;
    throw std::runtime_error("gzclose failed");
  }
  gz_ = nullptr;
}

} // namespace archive
