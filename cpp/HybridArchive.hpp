#pragma once

#include "HybridArchiveSpec.hpp"

namespace margelo::nitro::archive {

class HybridArchive : public HybridArchiveSpec {
public:
  HybridArchive() : HybridObject(TAG) {}

  std::shared_ptr<Promise<ArchiveResult>> unpack(
    const std::string& sourcePath,
    const std::string& targetPath,
    bool overwrite,
    const std::optional<ArchiveOptions>& options,
    const std::optional<std::function<void(double, double)>>& onProgress) override;

  std::shared_ptr<Promise<ArchiveResult>> pack(
    const std::string& sourcePath,
    const std::string& targetPath,
    ArchiveFormat format,
    const std::optional<ArchiveOptions>& options,
    const std::optional<std::function<void(double, double)>>& onProgress) override;

  std::shared_ptr<Promise<std::vector<ArchiveEntry>>> listContents(
    const std::string& sourcePath,
    const std::optional<ArchiveOptions>& options) override;

  std::shared_ptr<Promise<ArchiveFormat>> detectFormat(
    const std::string& sourcePath) override;

  std::shared_ptr<Promise<bool>> testArchive(
    const std::string& sourcePath,
    const std::optional<std::string>& password) override;
};

} // namespace margelo::nitro::archive
