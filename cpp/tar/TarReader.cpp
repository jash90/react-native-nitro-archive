#include "TarReader.hpp"
#include <cstring>
#include <stdexcept>

namespace archive {

TarReader::TarReader(DataSource source) : source_(std::move(source)) {}

bool TarReader::readBlock(char* block) {
  int total = 0;
  while (total < static_cast<int>(BLOCK_SIZE)) {
    int n = source_(block + total, static_cast<int>(BLOCK_SIZE) - total);
    if (n <= 0) return false;
    total += n;
  }
  return true;
}

bool TarReader::isZeroBlock(const char* block) {
  for (size_t i = 0; i < BLOCK_SIZE; i++) {
    if (block[i] != 0) return false;
  }
  return true;
}

size_t TarReader::parseOctal(const char* buf, int len) {
  // Skip leading spaces/zeros
  size_t result = 0;
  for (int i = 0; i < len && buf[i] != 0; i++) {
    if (buf[i] >= '0' && buf[i] <= '7') {
      result = result * 8 + static_cast<size_t>(buf[i] - '0');
    }
  }
  return result;
}

size_t TarReader::alignToBlock(size_t size) {
  size_t remainder = size % BLOCK_SIZE;
  return remainder == 0 ? size : size + (BLOCK_SIZE - remainder);
}

TarEntry TarReader::parseHeader(const char* header, std::string& longName, std::string& longLink) {
  TarEntry entry;

  // Name: bytes 0-99
  if (!longName.empty()) {
    entry.path = longName;
    longName.clear();
  } else {
    char nameBuf[101] = {0};
    std::memcpy(nameBuf, header, 100);
    entry.path = std::string(nameBuf);
  }

  // Size: bytes 124-135 (octal)
  entry.size = parseOctal(header + 124, 12);

  // Type: byte 156
  char typeFlag = header[156];
  entry.isDirectory = (typeFlag == '5');

  // Prefix: bytes 345-499 (POSIX/ustar)
  char magic[7] = {0};
  std::memcpy(magic, header + 257, 5);
  if (std::string(magic) == "ustar") {
    char prefixBuf[156] = {0};
    std::memcpy(prefixBuf, header + 345, 155);
    std::string prefix(prefixBuf);
    if (!prefix.empty()) {
      entry.path = prefix + "/" + entry.path;
    }
  }

  return entry;
}

void TarReader::iterate(TarEntryCallback callback) {
  char block[BLOCK_SIZE];
  int zeroBlockCount = 0;
  std::string longName;
  std::string longLink;

  while (readBlock(block)) {
    if (isZeroBlock(block)) {
      zeroBlockCount++;
      if (zeroBlockCount >= 2) break;
      continue;
    }
    zeroBlockCount = 0;

    // Parse header
    char typeFlag = block[156];
    size_t entrySize = parseOctal(block + 124, 12);

    // GNU long name (type 'L')
    if (typeFlag == 'L') {
      size_t aligned = alignToBlock(entrySize);
      std::vector<char> nameData(aligned);
      int total = 0;
      while (total < static_cast<int>(aligned)) {
        int n = source_(nameData.data() + total, static_cast<int>(aligned) - total);
        if (n <= 0) throw std::runtime_error("Unexpected EOF reading long name");
        total += n;
      }
      longName = std::string(nameData.data(), entrySize > 0 ? entrySize - 1 : 0); // strip trailing null
      continue;
    }

    // GNU long link (type 'K')
    if (typeFlag == 'K') {
      size_t aligned = alignToBlock(entrySize);
      std::vector<char> linkData(aligned);
      int total = 0;
      while (total < static_cast<int>(aligned)) {
        int n = source_(linkData.data() + total, static_cast<int>(aligned) - total);
        if (n <= 0) throw std::runtime_error("Unexpected EOF reading long link");
        total += n;
      }
      longLink = std::string(linkData.data(), entrySize > 0 ? entrySize - 1 : 0);
      continue;
    }

    auto entry = parseHeader(block, longName, longLink);

    if (entry.isDirectory || entry.size == 0) {
      if (!callback(entry, nullptr)) return;
    } else {
      // Read file data
      size_t aligned = alignToBlock(entry.size);
      std::vector<char> fileData(aligned);
      int total = 0;
      while (total < static_cast<int>(aligned)) {
        int n = source_(fileData.data() + total, static_cast<int>(aligned) - total);
        if (n <= 0) throw std::runtime_error("Unexpected EOF reading file data for: " + entry.path);
        total += n;
      }
      if (!callback(entry, fileData.data())) return;
    }
  }
}

} // namespace archive
