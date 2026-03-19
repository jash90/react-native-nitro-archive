#include "HybridTarBz2.hpp"
#include "bzlib.h"

#include <fstream>
#include <filesystem>
#include <cstring>
#include <ctime>
#include <algorithm>

namespace fs = std::filesystem;

namespace margelo::nitro::tarbz2 {

// MARK: - Public API

std::shared_ptr<Promise<TarBz2Result>> HybridTarBz2::unpack(
    const std::string& sourcePath, const std::string& targetPath, bool overwrite) {
  return Promise<TarBz2Result>::async([this, sourcePath, targetPath, overwrite]() -> TarBz2Result {
    try {
      auto compressed = readFile(sourcePath);
      if (compressed.empty()) {
        return TarBz2Result(false, "", "Cannot read source file: " + sourcePath);
      }

      auto tarData = bz2Decompress(compressed);
      if (tarData.empty()) {
        return TarBz2Result(false, "", "BZip2 decompression failed");
      }

      createDirectories(targetPath);
      extractTar(tarData, targetPath, overwrite);

      return TarBz2Result(true, targetPath, "");
    } catch (const std::exception& e) {
      return TarBz2Result(false, "", std::string("Unpack error: ") + e.what());
    }
  });
}

std::shared_ptr<Promise<TarBz2Result>> HybridTarBz2::pack(
    const std::string& sourcePath, const std::string& targetPath) {
  return Promise<TarBz2Result>::async([this, sourcePath, targetPath]() -> TarBz2Result {
    try {
      if (!fileExists(sourcePath)) {
        return TarBz2Result(false, "", "Source path does not exist: " + sourcePath);
      }

      auto tarData = createTar(sourcePath);
      auto bz2Data = bz2Compress(tarData);
      if (bz2Data.empty()) {
        return TarBz2Result(false, "", "BZip2 compression failed");
      }

      // Create parent directory if needed
      auto parent = parentPath(targetPath);
      if (!parent.empty()) {
        createDirectories(parent);
      }

      writeFile(targetPath, bz2Data.data(), bz2Data.size());
      return TarBz2Result(true, targetPath, "");
    } catch (const std::exception& e) {
      return TarBz2Result(false, "", std::string("Pack error: ") + e.what());
    }
  });
}

std::shared_ptr<Promise<std::vector<std::string>>> HybridTarBz2::listContents(
    const std::string& sourcePath) {
  return Promise<std::vector<std::string>>::async([this, sourcePath]() -> std::vector<std::string> {
    auto compressed = readFile(sourcePath);
    if (compressed.empty()) {
      throw std::runtime_error("Cannot read file: " + sourcePath);
    }

    auto tarData = bz2Decompress(compressed);
    if (tarData.empty()) {
      throw std::runtime_error("BZip2 decompression failed");
    }

    return listTarEntries(tarData);
  });
}

std::shared_ptr<Promise<TarBz2Result>> HybridTarBz2::unpackWithProgress(
    const std::string& sourcePath, const std::string& targetPath, bool overwrite,
    const std::function<void(double)>& onProgress) {
  return Promise<TarBz2Result>::async([this, sourcePath, targetPath, overwrite, onProgress]() -> TarBz2Result {
    try {
      if (onProgress) onProgress(0.0);
      auto compressed = readFile(sourcePath);
      if (compressed.empty()) {
        return TarBz2Result(false, "", "Cannot read source file: " + sourcePath);
      }
      if (onProgress) onProgress(0.1);

      auto tarData = bz2Decompress(compressed);
      if (tarData.empty()) {
        return TarBz2Result(false, "", "BZip2 decompression failed");
      }
      if (onProgress) onProgress(0.3);

      createDirectories(targetPath);
      extractTar(tarData, targetPath, overwrite);

      if (onProgress) onProgress(1.0);
      return TarBz2Result(true, targetPath, "");
    } catch (const std::exception& e) {
      return TarBz2Result(false, "", std::string("Unpack error: ") + e.what());
    }
  });
}

std::shared_ptr<Promise<TarBz2Result>> HybridTarBz2::packWithProgress(
    const std::string& sourcePath, const std::string& targetPath,
    const std::function<void(double)>& onProgress) {
  return Promise<TarBz2Result>::async([this, sourcePath, targetPath, onProgress]() -> TarBz2Result {
    try {
      if (!fileExists(sourcePath)) {
        return TarBz2Result(false, "", "Source path does not exist: " + sourcePath);
      }
      if (onProgress) onProgress(0.0);

      auto tarData = createTar(sourcePath);
      if (onProgress) onProgress(0.7);

      auto bz2Data = bz2Compress(tarData);
      if (bz2Data.empty()) {
        return TarBz2Result(false, "", "BZip2 compression failed");
      }
      if (onProgress) onProgress(0.9);

      auto parent = parentPath(targetPath);
      if (!parent.empty()) createDirectories(parent);

      writeFile(targetPath, bz2Data.data(), bz2Data.size());
      if (onProgress) onProgress(1.0);
      return TarBz2Result(true, targetPath, "");
    } catch (const std::exception& e) {
      return TarBz2Result(false, "", std::string("Pack error: ") + e.what());
    }
  });
}

// MARK: - BZip2

std::vector<uint8_t> HybridTarBz2::bz2Decompress(const std::vector<uint8_t>& data) {
  if (data.empty()) return {};

  unsigned int destLen = static_cast<unsigned int>(data.size() * 4);
  if (destLen < 4096) destLen = 4096;

  std::vector<uint8_t> dest(destLen);

  for (int attempt = 0; attempt < 8; attempt++) {
    unsigned int tryLen = static_cast<unsigned int>(dest.size());
    int ret = BZ2_bzBuffToBuffDecompress(
      reinterpret_cast<char*>(dest.data()), &tryLen,
      const_cast<char*>(reinterpret_cast<const char*>(data.data())),
      static_cast<unsigned int>(data.size()),
      0, 0
    );

    if (ret == BZ_OK) {
      dest.resize(tryLen);
      return dest;
    }

    if (ret == BZ_OUTBUFF_FULL) {
      dest.resize(dest.size() * 2);
      continue;
    }

    return {};
  }

  return {};
}

std::vector<uint8_t> HybridTarBz2::bz2Compress(const std::vector<uint8_t>& data) {
  if (data.empty()) return {};

  unsigned int destLen = static_cast<unsigned int>(data.size() + data.size() / 100 + 600);
  if (destLen < 1024) destLen = 1024;

  std::vector<uint8_t> dest(destLen);

  int ret = BZ2_bzBuffToBuffCompress(
    reinterpret_cast<char*>(dest.data()), &destLen,
    const_cast<char*>(reinterpret_cast<const char*>(data.data())),
    static_cast<unsigned int>(data.size()),
    9, 0, 30
  );

  if (ret != BZ_OK) return {};

  dest.resize(destLen);
  return dest;
}

// MARK: - Tar

HybridTarBz2::TarEntry HybridTarBz2::parseTarHeader(const uint8_t* header) {
  TarEntry entry;

  // Name: bytes 0-99
  char nameBuf[101] = {0};
  std::memcpy(nameBuf, header, 100);
  entry.name = std::string(nameBuf);

  // Size: bytes 124-135 (octal)
  char sizeBuf[13] = {0};
  std::memcpy(sizeBuf, header + 124, 12);
  entry.size = static_cast<size_t>(std::strtoul(sizeBuf, nullptr, 8));

  // Type: byte 156
  entry.isDirectory = (header[156] == '5');

  // Prefix: bytes 345-499 (POSIX/ustar)
  char prefixBuf[156] = {0};
  std::memcpy(prefixBuf, header + 345, 155);
  std::string prefix(prefixBuf);
  if (!prefix.empty()) {
    entry.name = prefix + "/" + entry.name;
  }

  return entry;
}

std::vector<uint8_t> HybridTarBz2::createTarHeader(const std::string& name, size_t size, bool isDir) {
  std::vector<uint8_t> header(TAR_BLOCK_SIZE, 0);

  // Name (0-99)
  std::memcpy(header.data(), name.c_str(), std::min(name.size(), size_t(100)));

  // Mode (100-107)
  auto writeOctal = [&](const char* val, int offset, int len) {
    int vlen = static_cast<int>(std::strlen(val));
    int pad = len - 1 - vlen;
    for (int i = 0; i < pad; i++) header[offset + i] = '0';
    std::memcpy(header.data() + offset + pad, val, vlen);
    header[offset + len - 1] = 0;
  };

  writeOctal(isDir ? "755" : "644", 100, 8);
  writeOctal("0", 108, 8);  // UID
  writeOctal("0", 116, 8);  // GID

  // Size (124-135)
  char sizeBuf[32];
  std::snprintf(sizeBuf, sizeof(sizeBuf), "%lo", static_cast<unsigned long>(size));
  writeOctal(sizeBuf, 124, 12);

  // Mtime (136-147)
  char mtimeBuf[32];
  std::snprintf(mtimeBuf, sizeof(mtimeBuf), "%lo", static_cast<unsigned long>(std::time(nullptr)));
  writeOctal(mtimeBuf, 136, 12);

  // Type flag (156)
  header[156] = isDir ? '5' : '0';

  // Magic "ustar\0" (257-262) + version "00" (263-264)
  std::memcpy(header.data() + 257, "ustar", 6);
  header[263] = '0';
  header[264] = '0';

  // Checksum (148-155): fill with spaces first
  std::memset(header.data() + 148, ' ', 8);
  uint32_t checksum = 0;
  for (auto b : header) checksum += b;
  char csumBuf[32];
  std::snprintf(csumBuf, sizeof(csumBuf), "%06o", checksum);
  std::memcpy(header.data() + 148, csumBuf, 6);
  header[154] = 0;
  header[155] = ' ';

  return header;
}

void HybridTarBz2::extractTar(const std::vector<uint8_t>& tarData, const std::string& targetDir, bool overwrite) {
  size_t offset = 0;

  while (offset + TAR_BLOCK_SIZE <= tarData.size()) {
    const uint8_t* headerPtr = tarData.data() + offset;

    // Check for end-of-archive (all zeros)
    bool allZero = true;
    for (size_t i = 0; i < TAR_BLOCK_SIZE; i++) {
      if (headerPtr[i] != 0) { allZero = false; break; }
    }
    if (allZero) break;

    auto entry = parseTarHeader(headerPtr);
    if (entry.name.empty()) break;
    offset += TAR_BLOCK_SIZE;

    auto fullPath = fs::path(targetDir) / entry.name;

    // Security: prevent path traversal
    auto canonical = fs::weakly_canonical(fullPath).string();
    auto targetCanonical = fs::weakly_canonical(fs::path(targetDir)).string();
    if (canonical.substr(0, targetCanonical.size()) != targetCanonical) {
      throw std::runtime_error("Entry outside target directory: " + entry.name);
    }

    if (entry.isDirectory) {
      fs::create_directories(fullPath);
    } else {
      if (!overwrite && fs::exists(fullPath)) {
        offset += alignToBlock(entry.size);
        continue;
      }

      fs::create_directories(fullPath.parent_path());

      if (entry.size > 0) {
        writeFile(fullPath.string(), tarData.data() + offset, entry.size);
        offset += alignToBlock(entry.size);
      } else {
        std::ofstream(fullPath.string()).close();
      }
    }

  }
}

std::vector<std::string> HybridTarBz2::listTarEntries(const std::vector<uint8_t>& tarData) {
  std::vector<std::string> entries;
  size_t offset = 0;

  while (offset + TAR_BLOCK_SIZE <= tarData.size()) {
    const uint8_t* headerPtr = tarData.data() + offset;

    bool allZero = true;
    for (size_t i = 0; i < TAR_BLOCK_SIZE; i++) {
      if (headerPtr[i] != 0) { allZero = false; break; }
    }
    if (allZero) break;

    auto entry = parseTarHeader(headerPtr);
    if (entry.name.empty()) break;

    entries.push_back(entry.name);
    offset += TAR_BLOCK_SIZE;

    if (!entry.isDirectory && entry.size > 0) {
      offset += alignToBlock(entry.size);
    }
  }

  return entries;
}

std::vector<uint8_t> HybridTarBz2::createTar(const std::string& sourcePath) {
  std::vector<uint8_t> tarData;
  fs::path source(sourcePath);

  auto appendData = [&](const std::vector<uint8_t>& d) {
    tarData.insert(tarData.end(), d.begin(), d.end());
  };
  auto appendPadding = [&](size_t dataSize) {
    size_t remainder = dataSize % TAR_BLOCK_SIZE;
    if (remainder > 0) {
      std::vector<uint8_t> pad(TAR_BLOCK_SIZE - remainder, 0);
      tarData.insert(tarData.end(), pad.begin(), pad.end());
    }
  };

  if (fs::is_directory(source)) {
    for (auto& entry : fs::recursive_directory_iterator(source)) {
      auto rel = fs::relative(entry.path(), source).string();

      if (entry.is_directory()) {
        appendData(createTarHeader(rel + "/", 0, true));
      } else if (entry.is_regular_file()) {
        auto fileData = readFile(entry.path().string());
        appendData(createTarHeader(rel, fileData.size(), false));
        tarData.insert(tarData.end(), fileData.begin(), fileData.end());
        appendPadding(fileData.size());
      }
    }
  } else {
    auto fileData = readFile(source.string());
    appendData(createTarHeader(source.filename().string(), fileData.size(), false));
    tarData.insert(tarData.end(), fileData.begin(), fileData.end());
    appendPadding(fileData.size());
  }

  // End-of-archive: two zero blocks
  std::vector<uint8_t> endMarker(TAR_BLOCK_SIZE * 2, 0);
  tarData.insert(tarData.end(), endMarker.begin(), endMarker.end());

  return tarData;
}

// MARK: - File Helpers

std::vector<uint8_t> HybridTarBz2::readFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) return {};

  // Get file size via filesystem
  auto fileSize = fs::file_size(path);
  if (fileSize == 0) return {};

  std::vector<uint8_t> data(static_cast<size_t>(fileSize));
  file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(fileSize));

  auto bytesRead = file.gcount();
  if (bytesRead < static_cast<std::streamsize>(fileSize)) {
    data.resize(static_cast<size_t>(bytesRead));
  }

  return data;
}

void HybridTarBz2::writeFile(const std::string& path, const uint8_t* data, size_t size) {
  std::ofstream file(path, std::ios::binary);
  if (!file) throw std::runtime_error("Cannot write file: " + path);
  file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
}

void HybridTarBz2::createDirectories(const std::string& path) {
  fs::create_directories(path);
}

bool HybridTarBz2::fileExists(const std::string& path) {
  return fs::exists(path);
}

bool HybridTarBz2::isDirectory(const std::string& path) {
  return fs::is_directory(path);
}

std::string HybridTarBz2::parentPath(const std::string& path) {
  return fs::path(path).parent_path().string();
}

} // namespace margelo::nitro::tarbz2
