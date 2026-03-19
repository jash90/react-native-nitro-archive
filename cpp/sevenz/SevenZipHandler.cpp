#include "SevenZipHandler.hpp"

#include "../vendor/lzma/7z.h"
#include "../vendor/lzma/7zAlloc.h"
#include "../vendor/lzma/7zCrc.h"
#include "../vendor/lzma/7zFile.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <memory>

namespace fs = std::filesystem;

namespace archive {

// Utility: Convert UTF-16 filename to UTF-8
static std::string utf16ToUtf8(const UInt16* src, size_t len) {
  std::string result;
  result.reserve(len);
  for (size_t i = 0; i < len; i++) {
    UInt16 c = src[i];
    if (c == 0) break;
    if (c < 0x80) {
      result.push_back(static_cast<char>(c));
    } else if (c < 0x800) {
      result.push_back(static_cast<char>(0xC0 | (c >> 6)));
      result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else {
      result.push_back(static_cast<char>(0xE0 | (c >> 12)));
      result.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
  }
  return result;
}

// Get filename from archive as UTF-8
static std::string getFileName(const CSzArEx* db, UInt32 index) {
  size_t len = SzArEx_GetFileNameUtf16(db, index, nullptr);
  if (len == 0) return "";
  std::vector<UInt16> buf(len);
  SzArEx_GetFileNameUtf16(db, index, buf.data());
  return utf16ToUtf8(buf.data(), len);
}

// RAII wrapper for 7z archive
struct SevenZipArchive {
  CFileInStream archiveStream;
  CLookToRead2 lookStream;
  CSzArEx db;
  ISzAlloc allocImp;
  ISzAlloc allocTempImp;
  Byte* lookBuf;

  SevenZipArchive() : lookBuf(nullptr) {
    allocImp.Alloc = SzAlloc;
    allocImp.Free = SzFree;
    allocTempImp.Alloc = SzAllocTemp;
    allocTempImp.Free = SzFreeTemp;
    SzArEx_Init(&db);
  }

  ~SevenZipArchive() {
    SzArEx_Free(&db, &allocImp);
    ISzAlloc_Free(&allocImp, lookBuf);
    File_Close(&archiveStream.file);
  }

  void open(const std::string& path) {
    static bool crcInitDone = false;
    if (!crcInitDone) {
      CrcGenerateTable();
      crcInitDone = true;
    }

    File_Construct(&archiveStream.file);
    WRes wres = InFile_Open(&archiveStream.file, path.c_str());
    if (wres != 0) {
      throw std::runtime_error("Cannot open 7z file: " + path);
    }

    FileInStream_CreateVTable(&archiveStream);

    static const size_t kBufSize = 1 << 18; // 256KB
    lookBuf = static_cast<Byte*>(ISzAlloc_Alloc(&allocImp, kBufSize));
    if (!lookBuf) {
      throw std::runtime_error("Out of memory");
    }

    LookToRead2_CreateVTable(&lookStream, False);
    lookStream.buf = lookBuf;
    lookStream.bufSize = kBufSize;
    lookStream.realStream = &archiveStream.vt;
    LookToRead2_Init(&lookStream);

    SRes res = SzArEx_Open(&db, &lookStream.vt, &allocImp, &allocTempImp);
    if (res != SZ_OK) {
      throw std::runtime_error("Failed to open 7z archive (error " + std::to_string(res) + ")");
    }
  }
};

// MARK: - Unpack

void SevenZipHandler::unpack(const std::string& src, const std::string& dst,
                              bool overwrite, const std::string& /*password*/,
                              const std::function<void(double, double)>& progress) {
  SevenZipArchive archive;
  archive.open(src);

  fs::create_directories(dst);
  auto dstCanonical = fs::weakly_canonical(fs::path(dst)).string();

  // Calculate total uncompressed size
  double totalSize = 0;
  for (UInt32 i = 0; i < archive.db.NumFiles; i++) {
    if (!SzArEx_IsDir(&archive.db, i)) {
      totalSize += static_cast<double>(SzArEx_GetFileSize(&archive.db, i));
    }
  }

  UInt32 blockIndex = 0xFFFFFFFF;
  Byte* outBuffer = nullptr;
  size_t outBufferSize = 0;
  double bytesExtracted = 0;

  for (UInt32 i = 0; i < archive.db.NumFiles; i++) {
    std::string name = getFileName(&archive.db, i);
    if (name.empty()) continue;

    // Replace backslashes with forward slashes
    for (auto& c : name) {
      if (c == '\\') c = '/';
    }

    auto fullPath = fs::path(dst) / name;

    // Security: prevent path traversal
    auto canonical = fs::weakly_canonical(fullPath).string();
    if (canonical.substr(0, dstCanonical.size()) != dstCanonical) {
      throw std::runtime_error("Entry outside target directory: " + name);
    }

    bool isDir = SzArEx_IsDir(&archive.db, i) != 0;

    if (isDir) {
      fs::create_directories(fullPath);
      continue;
    }

    if (!overwrite && fs::exists(fullPath)) {
      continue;
    }

    fs::create_directories(fullPath.parent_path());

    size_t offset = 0;
    size_t outSizeProcessed = 0;

    SRes res = SzArEx_Extract(&archive.db, &archive.lookStream.vt, i,
                               &blockIndex, &outBuffer, &outBufferSize,
                               &offset, &outSizeProcessed,
                               &archive.allocImp, &archive.allocTempImp);
    if (res != SZ_OK) {
      ISzAlloc_Free(&archive.allocImp, outBuffer);
      throw std::runtime_error("Failed to extract: " + name + " (error " + std::to_string(res) + ")");
    }

    std::ofstream out(fullPath.string(), std::ios::binary);
    if (!out) {
      ISzAlloc_Free(&archive.allocImp, outBuffer);
      throw std::runtime_error("Cannot write: " + fullPath.string());
    }
    if (outSizeProcessed > 0) {
      out.write(reinterpret_cast<const char*>(outBuffer + offset),
                static_cast<std::streamsize>(outSizeProcessed));
    }
    out.close();

    bytesExtracted += static_cast<double>(outSizeProcessed);
    if (progress && totalSize > 0) {
      progress(bytesExtracted, totalSize);
    }
  }

  ISzAlloc_Free(&archive.allocImp, outBuffer);
}

// MARK: - List

std::vector<SevenZipEntry> SevenZipHandler::list(const std::string& src) {
  SevenZipArchive archive;
  archive.open(src);

  std::vector<SevenZipEntry> entries;
  entries.reserve(archive.db.NumFiles);

  for (UInt32 i = 0; i < archive.db.NumFiles; i++) {
    SevenZipEntry entry;
    entry.path = getFileName(&archive.db, i);
    entry.size = SzArEx_GetFileSize(&archive.db, i);
    entry.isDirectory = SzArEx_IsDir(&archive.db, i) != 0;
    entries.push_back(std::move(entry));
  }

  return entries;
}

// MARK: - Test

bool SevenZipHandler::test(const std::string& src, const std::string& /*password*/) {
  try {
    SevenZipArchive archive;
    archive.open(src);

    UInt32 blockIndex = 0xFFFFFFFF;
    Byte* outBuffer = nullptr;
    size_t outBufferSize = 0;

    for (UInt32 i = 0; i < archive.db.NumFiles; i++) {
      if (SzArEx_IsDir(&archive.db, i)) continue;

      size_t offset = 0;
      size_t outSizeProcessed = 0;

      SRes res = SzArEx_Extract(&archive.db, &archive.lookStream.vt, i,
                                 &blockIndex, &outBuffer, &outBufferSize,
                                 &offset, &outSizeProcessed,
                                 &archive.allocImp, &archive.allocTempImp);
      if (res != SZ_OK) {
        ISzAlloc_Free(&archive.allocImp, outBuffer);
        return false;
      }
    }

    ISzAlloc_Free(&archive.allocImp, outBuffer);
    return true;
  } catch (...) {
    return false;
  }
}

} // namespace archive
