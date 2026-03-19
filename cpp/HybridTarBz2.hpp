#pragma once

#include "HybridTarBz2Spec.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace margelo::nitro::tarbz2 {

class HybridTarBz2 : public HybridTarBz2Spec {
public:
  HybridTarBz2() : HybridObject(TAG) {}

  // HybridTarBz2Spec methods
  std::shared_ptr<Promise<TarBz2Result>> unpack(const std::string& sourcePath, const std::string& targetPath, bool overwrite) override;
  std::shared_ptr<Promise<TarBz2Result>> pack(const std::string& sourcePath, const std::string& targetPath) override;
  std::shared_ptr<Promise<std::vector<std::string>>> listContents(const std::string& sourcePath) override;
  std::shared_ptr<Promise<TarBz2Result>> unpackWithProgress(const std::string& sourcePath, const std::string& targetPath, bool overwrite, const std::function<void(double)>& onProgress) override;
  std::shared_ptr<Promise<TarBz2Result>> packWithProgress(const std::string& sourcePath, const std::string& targetPath, const std::function<void(double)>& onProgress) override;

private:
  // BZip2
  std::vector<uint8_t> bz2Decompress(const std::vector<uint8_t>& data);
  std::vector<uint8_t> bz2Compress(const std::vector<uint8_t>& data);

  // Tar
  static constexpr size_t TAR_BLOCK_SIZE = 512;

  struct TarEntry {
    std::string name;
    size_t size;
    bool isDirectory;
  };

  TarEntry parseTarHeader(const uint8_t* header);
  std::vector<uint8_t> createTarHeader(const std::string& name, size_t size, bool isDirectory);
  void extractTar(const std::vector<uint8_t>& tarData, const std::string& targetDir, bool overwrite);
  std::vector<std::string> listTarEntries(const std::vector<uint8_t>& tarData);
  std::vector<uint8_t> createTar(const std::string& sourcePath);

  // File helpers
  std::vector<uint8_t> readFile(const std::string& path);
  void writeFile(const std::string& path, const uint8_t* data, size_t size);
  void createDirectories(const std::string& path);
  bool fileExists(const std::string& path);
  bool isDirectory(const std::string& path);
  std::vector<std::string> listFilesRecursive(const std::string& dir);
  std::string parentPath(const std::string& path);

  size_t alignToBlock(size_t size) {
    size_t remainder = size % TAR_BLOCK_SIZE;
    return remainder == 0 ? size : size + (TAR_BLOCK_SIZE - remainder);
  }
};

} // namespace margelo::nitro::tarbz2
