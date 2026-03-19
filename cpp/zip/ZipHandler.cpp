#include "ZipHandler.hpp"

#include "../vendor/minizip-ng/mz.h"
#include "../vendor/minizip-ng/mz_strm.h"
#include "../vendor/minizip-ng/mz_zip.h"
#include "../vendor/minizip-ng/mz_zip_rw.h"

#include <filesystem>
#include <stdexcept>
#include <cstring>

namespace fs = std::filesystem;

namespace archive {

// MARK: - Unpack

void ZipHandler::unpack(const std::string& src, const std::string& dst,
                        bool overwrite, const ZipOptions& opts,
                        const std::function<void(double, double)>& progress) {
  void* reader = mz_zip_reader_create();
  if (!reader) throw std::runtime_error("Failed to create zip reader");

  if (!opts.password.empty()) {
    mz_zip_reader_set_password(reader, opts.password.c_str());
  }

  // Set up overwrite callback
  if (!overwrite) {
    mz_zip_reader_set_overwrite_cb(reader, nullptr,
      [](void* /*handle*/, void* /*userdata*/, mz_zip_file* /*file_info*/, const char* /*path*/) -> int32_t {
        return MZ_EXIST_ERROR; // Skip existing files
      });
  }

  // Set up progress callback
  struct ProgressCtx {
    std::function<void(double, double)> cb;
    double totalSize;
    double processed;
  };

  ProgressCtx pctx;
  pctx.cb = progress;
  pctx.totalSize = 0;
  pctx.processed = 0;

  // Calculate total size
  if (fs::exists(src)) {
    pctx.totalSize = static_cast<double>(fs::file_size(src));
  }

  if (progress) {
    mz_zip_reader_set_progress_cb(reader, &pctx,
      [](void* /*handle*/, void* userdata, mz_zip_file* /*file_info*/, int64_t position) -> int32_t {
        auto* ctx = static_cast<ProgressCtx*>(userdata);
        if (ctx->cb) {
          ctx->cb(static_cast<double>(position), ctx->totalSize);
        }
        return MZ_OK;
      });
  }

  fs::create_directories(dst);

  // Resolve symlinks in destination path (e.g. /data/user/0 -> /data/data on Android)
  // to prevent false positives in minizip-ng's symlink safety check
  std::string resolvedDst = dst;
  auto canonical = fs::canonical(dst);
  resolvedDst = canonical.string();

  int32_t err = mz_zip_reader_open_file(reader, src.c_str());
  if (err != MZ_OK) {
    mz_zip_reader_delete(&reader);
    throw std::runtime_error("Cannot open zip file: " + src + " (error " + std::to_string(err) + ")");
  }

  err = mz_zip_reader_save_all(reader, resolvedDst.c_str());
  if (err != MZ_OK && err != MZ_END_OF_LIST) {
    mz_zip_reader_close(reader);
    mz_zip_reader_delete(&reader);
    if (err == MZ_PASSWORD_ERROR) {
      throw std::runtime_error("Wrong password for zip archive");
    }
    throw std::runtime_error("Failed to extract zip: error " + std::to_string(err));
  }

  mz_zip_reader_close(reader);
  mz_zip_reader_delete(&reader);
}

// MARK: - Pack

void ZipHandler::pack(const std::string& src, const std::string& dst,
                      const ZipOptions& opts,
                      const std::function<void(double, double)>& progress) {
  if (!fs::exists(src)) {
    throw std::runtime_error("Source path does not exist: " + src);
  }

  void* writer = mz_zip_writer_create();
  if (!writer) throw std::runtime_error("Failed to create zip writer");

  if (!opts.password.empty()) {
    mz_zip_writer_set_password(writer, opts.password.c_str());
    mz_zip_writer_set_aes(writer, 1); // Enable AES encryption
  }

  if (opts.compressionLevel >= 0 && opts.compressionLevel <= 9) {
    mz_zip_writer_set_compress_level(writer, static_cast<int16_t>(opts.compressionLevel));
  }

  mz_zip_writer_set_compress_method(writer, MZ_COMPRESS_METHOD_DEFLATE);

  // Calculate total size for progress
  double totalBytes = 0;
  if (fs::is_directory(src)) {
    for (auto& entry : fs::recursive_directory_iterator(src)) {
      if (entry.is_regular_file()) {
        totalBytes += static_cast<double>(entry.file_size());
      }
    }
  } else {
    totalBytes = static_cast<double>(fs::file_size(src));
  }

  struct ProgressCtx {
    std::function<void(double, double)> cb;
    double totalSize;
  };

  ProgressCtx pctx;
  pctx.cb = progress;
  pctx.totalSize = totalBytes;

  if (progress) {
    mz_zip_writer_set_progress_cb(writer, &pctx,
      [](void* /*handle*/, void* userdata, mz_zip_file* /*file_info*/, int64_t position) -> int32_t {
        auto* ctx = static_cast<ProgressCtx*>(userdata);
        if (ctx->cb) {
          ctx->cb(static_cast<double>(position), ctx->totalSize);
        }
        return MZ_OK;
      });
  }

  // Create parent directory
  auto parent = fs::path(dst).parent_path();
  if (!parent.empty()) fs::create_directories(parent);

  int32_t err = mz_zip_writer_open_file(writer, dst.c_str(), 0, 0);
  if (err != MZ_OK) {
    mz_zip_writer_delete(&writer);
    throw std::runtime_error("Cannot create zip file: " + dst);
  }

  fs::path srcPath(src);
  if (fs::is_directory(srcPath)) {
    err = mz_zip_writer_add_path(writer, src.c_str(), src.c_str(), 0, 1);
  } else {
    err = mz_zip_writer_add_file(writer, src.c_str(), srcPath.filename().c_str());
  }

  if (err != MZ_OK && err != MZ_END_OF_LIST) {
    mz_zip_writer_close(writer);
    mz_zip_writer_delete(&writer);
    throw std::runtime_error("Failed to add files to zip: error " + std::to_string(err));
  }

  mz_zip_writer_close(writer);
  mz_zip_writer_delete(&writer);
}

// MARK: - List

std::vector<ZipEntry> ZipHandler::list(const std::string& src, const ZipOptions& opts) {
  std::vector<ZipEntry> entries;

  void* reader = mz_zip_reader_create();
  if (!reader) throw std::runtime_error("Failed to create zip reader");

  if (!opts.password.empty()) {
    mz_zip_reader_set_password(reader, opts.password.c_str());
  }

  int32_t err = mz_zip_reader_open_file(reader, src.c_str());
  if (err != MZ_OK) {
    mz_zip_reader_delete(&reader);
    throw std::runtime_error("Cannot open zip file: " + src);
  }

  err = mz_zip_reader_goto_first_entry(reader);
  while (err == MZ_OK) {
    mz_zip_file* fileInfo = nullptr;
    if (mz_zip_reader_entry_get_info(reader, &fileInfo) == MZ_OK && fileInfo) {
      ZipEntry entry;
      entry.path = fileInfo->filename ? fileInfo->filename : "";
      entry.size = static_cast<uint64_t>(fileInfo->uncompressed_size);
      entry.compressedSize = static_cast<uint64_t>(fileInfo->compressed_size);
      entry.isDirectory = mz_zip_reader_entry_is_dir(reader) == MZ_OK;
      entry.isEncrypted = (fileInfo->flag & MZ_ZIP_FLAG_ENCRYPTED) != 0;
      entries.push_back(std::move(entry));
    }
    err = mz_zip_reader_goto_next_entry(reader);
  }

  mz_zip_reader_close(reader);
  mz_zip_reader_delete(&reader);

  return entries;
}

// MARK: - Test

bool ZipHandler::test(const std::string& src, const std::string& password) {
  void* reader = mz_zip_reader_create();
  if (!reader) return false;

  if (!password.empty()) {
    mz_zip_reader_set_password(reader, password.c_str());
  }

  int32_t err = mz_zip_reader_open_file(reader, src.c_str());
  if (err != MZ_OK) {
    mz_zip_reader_delete(&reader);
    return false;
  }

  bool valid = true;
  err = mz_zip_reader_goto_first_entry(reader);
  while (err == MZ_OK) {
    if (mz_zip_reader_entry_is_dir(reader) != MZ_OK) {
      // Try to open and read entry to verify integrity
      int32_t openErr = mz_zip_reader_entry_open(reader);
      if (openErr != MZ_OK) {
        valid = false;
        break;
      }
      char buf[4096];
      int32_t readResult;
      do {
        readResult = mz_zip_reader_entry_read(reader, buf, sizeof(buf));
        if (readResult < 0) {
          valid = false;
          break;
        }
      } while (readResult > 0);
      mz_zip_reader_entry_close(reader);
      if (!valid) break;
    }
    err = mz_zip_reader_goto_next_entry(reader);
  }

  mz_zip_reader_close(reader);
  mz_zip_reader_delete(&reader);
  return valid;
}

} // namespace archive
