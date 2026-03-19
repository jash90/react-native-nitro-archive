#include "TarWriter.hpp"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

namespace archive {

TarWriter::TarWriter(DataSink sink) : sink_(std::move(sink)) {}

void TarWriter::writeOctal(char* buf, int offset, int len, unsigned long value) {
  char tmp[32];
  std::snprintf(tmp, sizeof(tmp), "%0*lo", len - 1, value);
  std::memcpy(buf + offset, tmp, static_cast<size_t>(len - 1));
  buf[offset + len - 1] = 0;
}

void TarWriter::computeChecksum(char* header) {
  // Fill checksum field with spaces
  std::memset(header + 148, ' ', 8);
  uint32_t sum = 0;
  for (int i = 0; i < static_cast<int>(BLOCK_SIZE); i++) {
    sum += static_cast<uint8_t>(header[i]);
  }
  char csumBuf[8];
  std::snprintf(csumBuf, sizeof(csumBuf), "%06o", sum);
  std::memcpy(header + 148, csumBuf, 6);
  header[154] = 0;
  header[155] = ' ';
}

void TarWriter::writeLongName(const std::string& name) {
  // GNU long name: write a header with type 'L', then the name data
  size_t nameLen = name.size() + 1; // include null terminator
  char header[BLOCK_SIZE];
  std::memset(header, 0, BLOCK_SIZE);

  std::memcpy(header, "././@LongLink", 13);
  writeOctal(header, 100, 8, 0644);
  writeOctal(header, 108, 8, 0);
  writeOctal(header, 116, 8, 0);
  writeOctal(header, 124, 12, static_cast<unsigned long>(nameLen));
  writeOctal(header, 136, 12, static_cast<unsigned long>(std::time(nullptr)));
  header[156] = 'L'; // GNU long name type
  std::memcpy(header + 257, "ustar ", 6);
  header[263] = ' ';

  computeChecksum(header);
  sink_(header, static_cast<int>(BLOCK_SIZE));

  // Write name data + padding
  size_t aligned = nameLen;
  size_t remainder = aligned % BLOCK_SIZE;
  if (remainder > 0) aligned += (BLOCK_SIZE - remainder);

  std::vector<char> nameBlock(aligned, 0);
  std::memcpy(nameBlock.data(), name.c_str(), name.size() + 1);
  sink_(nameBlock.data(), static_cast<int>(aligned));
}

void TarWriter::writeHeader(const std::string& name, size_t size, bool isDir) {
  if (name.size() > 100) {
    writeLongName(name);
  }

  char header[BLOCK_SIZE];
  std::memset(header, 0, BLOCK_SIZE);

  // Name (0-99) - truncate if needed, long name header already written
  std::memcpy(header, name.c_str(), std::min(name.size(), size_t(100)));

  // Mode
  writeOctal(header, 100, 8, isDir ? 0755 : 0644);
  // UID
  writeOctal(header, 108, 8, 0);
  // GID
  writeOctal(header, 116, 8, 0);
  // Size
  writeOctal(header, 124, 12, static_cast<unsigned long>(size));
  // Mtime
  writeOctal(header, 136, 12, static_cast<unsigned long>(std::time(nullptr)));
  // Type
  header[156] = isDir ? '5' : '0';
  // Magic
  std::memcpy(header + 257, "ustar", 6);
  header[263] = '0';
  header[264] = '0';

  computeChecksum(header);
  sink_(header, static_cast<int>(BLOCK_SIZE));
}

void TarWriter::writePadding(size_t dataSize) {
  size_t remainder = dataSize % BLOCK_SIZE;
  if (remainder > 0) {
    size_t padLen = BLOCK_SIZE - remainder;
    std::vector<char> pad(padLen, 0);
    sink_(pad.data(), static_cast<int>(padLen));
  }
}

void TarWriter::writeFileEntry(const std::string& archiveName, const std::string& localPath) {
  auto fileSize = std::filesystem::file_size(localPath);
  writeHeader(archiveName, static_cast<size_t>(fileSize), false);

  // Stream file data
  std::ifstream file(localPath, std::ios::binary);
  if (!file) throw std::runtime_error("Cannot open file: " + localPath);

  char buf[8192];
  size_t totalWritten = 0;
  while (file && totalWritten < static_cast<size_t>(fileSize)) {
    auto toRead = std::min(sizeof(buf), static_cast<size_t>(fileSize) - totalWritten);
    file.read(buf, static_cast<std::streamsize>(toRead));
    auto bytesRead = file.gcount();
    if (bytesRead > 0) {
      sink_(buf, static_cast<int>(bytesRead));
      totalWritten += static_cast<size_t>(bytesRead);
    }
  }

  writePadding(static_cast<size_t>(fileSize));
}

void TarWriter::writeDirEntry(const std::string& archiveName) {
  std::string name = archiveName;
  if (!name.empty() && name.back() != '/') name += '/';
  writeHeader(name, 0, true);
}

void TarWriter::finish() {
  // Two zero blocks mark end of archive
  char zeros[BLOCK_SIZE];
  std::memset(zeros, 0, BLOCK_SIZE);
  sink_(zeros, static_cast<int>(BLOCK_SIZE));
  sink_(zeros, static_cast<int>(BLOCK_SIZE));
}

} // namespace archive
