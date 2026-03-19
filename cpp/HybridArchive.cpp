#include "HybridArchive.hpp"

#include "compression/BzipStream.hpp"
#include "compression/GzipStream.hpp"
#include "tar/TarReader.hpp"
#include "tar/TarWriter.hpp"
#include "zip/ZipHandler.hpp"
#include "sevenz/SevenZipHandler.hpp"
#include "utils/MagicBytes.hpp"

#include <filesystem>
#include <fstream>
#include <cstdio>
#include <stdexcept>
#include <memory>

namespace fs = std::filesystem;

namespace margelo::nitro::archive {

// Convert from internal MagicBytes enum to Nitrogen-generated enum
static ArchiveFormat fromMagicFormat(::archive::ArchiveFormat fmt) {
  switch (fmt) {
    case ::archive::ArchiveFormat::Tar: return ArchiveFormat::TAR;
    case ::archive::ArchiveFormat::Gz: return ArchiveFormat::GZ;
    case ::archive::ArchiveFormat::Bz2: return ArchiveFormat::BZ2;
    case ::archive::ArchiveFormat::TarGz: return ArchiveFormat::TAR_GZ;
    case ::archive::ArchiveFormat::TarBz2: return ArchiveFormat::TAR_BZ2;
    case ::archive::ArchiveFormat::Zip: return ArchiveFormat::ZIP;
    case ::archive::ArchiveFormat::SevenZ: return ArchiveFormat::SEVENZ;
    default: return ArchiveFormat::TAR_BZ2;
  }
}

// Extract options helpers
static std::string getPassword(const std::optional<ArchiveOptions>& options) {
  if (options && options->password.has_value()) {
    return options->password.value();
  }
  return "";
}

static int getCompressionLevel(const std::optional<ArchiveOptions>& options) {
  if (options && options->compressionLevel.has_value()) {
    return static_cast<int>(options->compressionLevel.value());
  }
  return 6; // default
}

// MARK: - detectFormat

std::shared_ptr<Promise<ArchiveFormat>> HybridArchive::detectFormat(
    const std::string& sourcePath) {
  return Promise<ArchiveFormat>::async([sourcePath]() -> ArchiveFormat {
    return fromMagicFormat(::archive::detectFormat(sourcePath));
  });
}

// MARK: - testArchive

std::shared_ptr<Promise<bool>> HybridArchive::testArchive(
    const std::string& sourcePath,
    const std::optional<std::string>& password) {
  return Promise<bool>::async([sourcePath, password]() -> bool {
    auto fmt = fromMagicFormat(::archive::detectFormat(sourcePath));
    std::string pwd = password.value_or("");

    switch (fmt) {
      case ArchiveFormat::ZIP:
        return ::archive::ZipHandler::test(sourcePath, pwd);
      case ArchiveFormat::SEVENZ:
        return ::archive::SevenZipHandler::test(sourcePath, pwd);
      default: {
        // For tar/gz/bz2 — just try to open and read
        try {
          FILE* fp = std::fopen(sourcePath.c_str(), "rb");
          if (!fp) return false;
          char buf[512];
          bool ok = std::fread(buf, 1, sizeof(buf), fp) > 0;
          std::fclose(fp);
          return ok;
        } catch (...) {
          return false;
        }
      }
    }
  });
}

// MARK: - listContents

std::shared_ptr<Promise<std::vector<ArchiveEntry>>> HybridArchive::listContents(
    const std::string& sourcePath,
    const std::optional<ArchiveOptions>& options) {
  return Promise<std::vector<ArchiveEntry>>::async([sourcePath, options]() -> std::vector<ArchiveEntry> {
    auto fmt = fromMagicFormat(::archive::detectFormat(sourcePath));
    std::vector<ArchiveEntry> entries;

    switch (fmt) {
      case ArchiveFormat::ZIP: {
        ::archive::ZipOptions opts;
        opts.password = getPassword(options);
        auto zipEntries = ::archive::ZipHandler::list(sourcePath, opts);
        for (auto& ze : zipEntries) {
          ArchiveEntry ae;
          ae.path = ze.path;
          ae.size = static_cast<double>(ze.size);
          ae.isDirectory = ze.isDirectory;
          ae.compressedSize = static_cast<double>(ze.compressedSize);
          ae.isEncrypted = ze.isEncrypted;
          entries.push_back(std::move(ae));
        }
        return entries;
      }
      case ArchiveFormat::SEVENZ: {
        auto szEntries = ::archive::SevenZipHandler::list(sourcePath);
        for (auto& se : szEntries) {
          ArchiveEntry ae;
          ae.path = se.path;
          ae.size = static_cast<double>(se.size);
          ae.isDirectory = se.isDirectory;
          ae.compressedSize = 0;
          ae.isEncrypted = false;
          entries.push_back(std::move(ae));
        }
        return entries;
      }
      default:
        break;
    }

    // Tar-based formats
    auto iterateTar = [&](::archive::DataSource source) {
      ::archive::TarReader reader(std::move(source));
      reader.iterate([&](const ::archive::TarEntry& entry, const char*) -> bool {
        ArchiveEntry ae;
        ae.path = entry.path;
        ae.size = static_cast<double>(entry.size);
        ae.isDirectory = entry.isDirectory;
        ae.compressedSize = 0;
        ae.isEncrypted = false;
        entries.push_back(std::move(ae));
        return true;
      });
    };

    switch (fmt) {
      case ArchiveFormat::TAR_BZ2: {
        FILE* fp = std::fopen(sourcePath.c_str(), "rb");
        if (!fp) throw std::runtime_error("Cannot open file: " + sourcePath);
        ::archive::BzipReader bz(fp);
        iterateTar([&bz](char* buf, int len) { return bz.read(buf, len); });
        std::fclose(fp);
        break;
      }
      case ArchiveFormat::TAR_GZ: {
        ::archive::GzipReader gz(sourcePath);
        iterateTar([&gz](char* buf, int len) { return gz.read(buf, len); });
        break;
      }
      case ArchiveFormat::TAR: {
        FILE* fp = std::fopen(sourcePath.c_str(), "rb");
        if (!fp) throw std::runtime_error("Cannot open file: " + sourcePath);
        iterateTar([fp](char* buf, int len) -> int {
          return static_cast<int>(std::fread(buf, 1, static_cast<size_t>(len), fp));
        });
        std::fclose(fp);
        break;
      }
      default:
        throw std::runtime_error("Unsupported format for listContents");
    }

    return entries;
  });
}

// MARK: - unpack

std::shared_ptr<Promise<ArchiveResult>> HybridArchive::unpack(
    const std::string& sourcePath,
    const std::string& targetPath,
    bool overwrite,
    const std::optional<ArchiveOptions>& options,
    const std::optional<std::function<void(double, double)>>& onProgress) {
  return Promise<ArchiveResult>::async([sourcePath, targetPath, overwrite, options, onProgress]() -> ArchiveResult {
    try {
      auto fmt = fromMagicFormat(::archive::detectFormat(sourcePath));

      // ZIP
      if (fmt == ArchiveFormat::ZIP) {
        ::archive::ZipOptions opts;
        opts.password = getPassword(options);
        opts.compressionLevel = getCompressionLevel(options);
        std::function<void(double, double)> progressFn;
        if (onProgress && *onProgress) progressFn = *onProgress;
        ::archive::ZipHandler::unpack(sourcePath, targetPath, overwrite, opts, progressFn);
        double bytesProcessed = 0;
        if (fs::exists(sourcePath)) bytesProcessed = static_cast<double>(fs::file_size(sourcePath));
        return ArchiveResult{true, targetPath, "", bytesProcessed};
      }

      // 7z
      if (fmt == ArchiveFormat::SEVENZ) {
        std::string pwd = getPassword(options);
        std::function<void(double, double)> progressFn;
        if (onProgress && *onProgress) progressFn = *onProgress;
        ::archive::SevenZipHandler::unpack(sourcePath, targetPath, overwrite, pwd, progressFn);
        double bytesProcessed = 0;
        if (fs::exists(sourcePath)) bytesProcessed = static_cast<double>(fs::file_size(sourcePath));
        return ArchiveResult{true, targetPath, "", bytesProcessed};
      }

      // Tar-based formats (existing code)
      double totalBytes = 0;
      if (fs::exists(sourcePath)) {
        totalBytes = static_cast<double>(fs::file_size(sourcePath));
      }
      double bytesRead = 0;

      auto reportProgress = [&]() {
        if (onProgress && *onProgress && totalBytes > 0) {
          (*onProgress)(bytesRead, totalBytes);
        }
      };

      fs::create_directories(targetPath);
      auto targetCanonical = fs::weakly_canonical(fs::path(targetPath)).string();

      auto extractTar = [&](::archive::DataSource source) {
        ::archive::TarReader reader(std::move(source));
        reader.iterate([&](const ::archive::TarEntry& entry, const char* data) -> bool {
          auto fullPath = fs::path(targetPath) / entry.path;

          // Security: prevent path traversal
          auto canonical = fs::weakly_canonical(fullPath).string();
          if (canonical.substr(0, targetCanonical.size()) != targetCanonical) {
            throw std::runtime_error("Entry outside target directory: " + entry.path);
          }

          if (entry.isDirectory) {
            fs::create_directories(fullPath);
          } else {
            if (!overwrite && fs::exists(fullPath)) {
              return true;
            }
            fs::create_directories(fullPath.parent_path());
            if (entry.size > 0 && data) {
              std::ofstream out(fullPath.string(), std::ios::binary);
              if (!out) throw std::runtime_error("Cannot write: " + fullPath.string());
              out.write(data, static_cast<std::streamsize>(entry.size));
            } else {
              std::ofstream(fullPath.string()).close();
            }
          }
          bytesRead += static_cast<double>(entry.size);
          reportProgress();
          return true;
        });
      };

      switch (fmt) {
        case ArchiveFormat::TAR_BZ2: {
          FILE* fp = std::fopen(sourcePath.c_str(), "rb");
          if (!fp) throw std::runtime_error("Cannot open: " + sourcePath);
          ::archive::BzipReader bz(fp);
          extractTar([&bz](char* buf, int len) { return bz.read(buf, len); });
          std::fclose(fp);
          break;
        }
        case ArchiveFormat::TAR_GZ: {
          ::archive::GzipReader gz(sourcePath);
          extractTar([&gz](char* buf, int len) { return gz.read(buf, len); });
          break;
        }
        case ArchiveFormat::TAR: {
          FILE* fp = std::fopen(sourcePath.c_str(), "rb");
          if (!fp) throw std::runtime_error("Cannot open: " + sourcePath);
          extractTar([fp](char* buf, int len) -> int {
            return static_cast<int>(std::fread(buf, 1, static_cast<size_t>(len), fp));
          });
          std::fclose(fp);
          break;
        }
        default:
          return ArchiveResult{false, "", "Unsupported archive format", 0};
      }

      return ArchiveResult{true, targetPath, "", bytesRead};
    } catch (const std::exception& e) {
      return ArchiveResult{false, "", std::string("Unpack error: ") + e.what(), 0};
    }
  });
}

// MARK: - pack

std::shared_ptr<Promise<ArchiveResult>> HybridArchive::pack(
    const std::string& sourcePath,
    const std::string& targetPath,
    ArchiveFormat format,
    const std::optional<ArchiveOptions>& options,
    const std::optional<std::function<void(double, double)>>& onProgress) {
  return Promise<ArchiveResult>::async([sourcePath, targetPath, format, options, onProgress]() -> ArchiveResult {
    try {
      if (!fs::exists(sourcePath)) {
        return ArchiveResult{false, "", "Source path does not exist: " + sourcePath, 0};
      }

      // ZIP
      if (format == ArchiveFormat::ZIP) {
        ::archive::ZipOptions opts;
        opts.password = getPassword(options);
        opts.compressionLevel = getCompressionLevel(options);
        std::function<void(double, double)> progressFn;
        if (onProgress && *onProgress) progressFn = *onProgress;
        ::archive::ZipHandler::pack(sourcePath, targetPath, opts, progressFn);
        double bytesProcessed = 0;
        if (fs::is_directory(sourcePath)) {
          for (auto& entry : fs::recursive_directory_iterator(sourcePath)) {
            if (entry.is_regular_file()) bytesProcessed += static_cast<double>(entry.file_size());
          }
        } else {
          bytesProcessed = static_cast<double>(fs::file_size(sourcePath));
        }
        return ArchiveResult{true, targetPath, "", bytesProcessed};
      }

      // 7z — write not supported
      if (format == ArchiveFormat::SEVENZ) {
        return ArchiveResult{false, "", "7z write not supported; use zip or tar formats", 0};
      }

      // Tar-based formats (existing code)
      double totalBytes = 0;
      if (fs::is_directory(sourcePath)) {
        for (auto& entry : fs::recursive_directory_iterator(sourcePath)) {
          if (entry.is_regular_file()) {
            totalBytes += static_cast<double>(entry.file_size());
          }
        }
      } else {
        totalBytes = static_cast<double>(fs::file_size(sourcePath));
      }

      double bytesWritten = 0;
      auto reportProgress = [&]() {
        if (onProgress && *onProgress && totalBytes > 0) {
          (*onProgress)(bytesWritten, totalBytes);
        }
      };

      // Create parent directory
      auto parent = fs::path(targetPath).parent_path();
      if (!parent.empty()) fs::create_directories(parent);

      // Set up the compression sink
      FILE* outFp = nullptr;
      std::unique_ptr<::archive::BzipWriter> bzWriter;
      std::unique_ptr<::archive::GzipWriter> gzWriter;

      ::archive::DataSink sink;

      bool needsTar = (format == ArchiveFormat::TAR ||
                       format == ArchiveFormat::TAR_BZ2 ||
                       format == ArchiveFormat::TAR_GZ);

      switch (format) {
        case ArchiveFormat::TAR_BZ2:
        case ArchiveFormat::BZ2: {
          outFp = std::fopen(targetPath.c_str(), "wb");
          if (!outFp) throw std::runtime_error("Cannot create: " + targetPath);
          bzWriter = std::make_unique<::archive::BzipWriter>(outFp);
          sink = [&bzWriter](const char* buf, int len) {
            bzWriter->write(buf, len);
          };
          break;
        }
        case ArchiveFormat::TAR_GZ:
        case ArchiveFormat::GZ: {
          gzWriter = std::make_unique<::archive::GzipWriter>(targetPath);
          sink = [&gzWriter](const char* buf, int len) {
            gzWriter->write(buf, len);
          };
          break;
        }
        case ArchiveFormat::TAR: {
          outFp = std::fopen(targetPath.c_str(), "wb");
          if (!outFp) throw std::runtime_error("Cannot create: " + targetPath);
          sink = [outFp](const char* buf, int len) {
            std::fwrite(buf, 1, static_cast<size_t>(len), outFp);
          };
          break;
        }
        default:
          break;
      }

      if (needsTar) {
        ::archive::TarWriter tarWriter(sink);
        fs::path source(sourcePath);

        if (fs::is_directory(source)) {
          for (auto& entry : fs::recursive_directory_iterator(source)) {
            auto rel = fs::relative(entry.path(), source).string();
            if (entry.is_directory()) {
              tarWriter.writeDirEntry(rel);
            } else if (entry.is_regular_file()) {
              tarWriter.writeFileEntry(rel, entry.path().string());
              bytesWritten += static_cast<double>(entry.file_size());
              reportProgress();
            }
          }
        } else {
          tarWriter.writeFileEntry(source.filename().string(), source.string());
          bytesWritten = totalBytes;
          reportProgress();
        }
        tarWriter.finish();
      } else {
        // Plain bz2 or gz compression (single file)
        if (fs::is_directory(sourcePath)) {
          return ArchiveResult{false, "", "Cannot compress directory without tar. Use tar.bz2 or tar.gz", 0};
        }
        std::ifstream in(sourcePath, std::ios::binary);
        if (!in) throw std::runtime_error("Cannot open: " + sourcePath);
        char buf[8192];
        while (in) {
          in.read(buf, sizeof(buf));
          auto n = in.gcount();
          if (n > 0) {
            sink(buf, static_cast<int>(n));
            bytesWritten += static_cast<double>(n);
            reportProgress();
          }
        }
      }

      // Finalize compression
      if (bzWriter) bzWriter->finish();
      if (gzWriter) gzWriter->finish();
      if (outFp) std::fclose(outFp);

      return ArchiveResult{true, targetPath, "", bytesWritten};
    } catch (const std::exception& e) {
      return ArchiveResult{false, "", std::string("Pack error: ") + e.what(), 0};
    }
  });
}

} // namespace margelo::nitro::archive
