#include "package.h"

#include <absl/log/log.h>

#include "compressor.h"

namespace pack {

Package::Package(const std::filesystem::path& path) : database_(path) {
  if (!database_.IsValid()) {
    return;
  }
  is_valid_ = true;
}

Package::~Package() = default;

bool Package::IsValid() const {
  return is_valid_;
}

bool Package::RegisterFilesInDirectory(const std::filesystem::path& path,
                                       const UniqueFD* base_directory) {
  if (!IterateDirectoryRecursively(
          [&](const std::string& file_path, const UniqueFD& fd) {
            const auto mapping = FileMapping::CreateReadOnly(fd);
            if (!mapping) {
              LOG(ERROR) << "Could not create file mapping for " << file_path;
              return false;
            }
            const auto hash = GetMappingHash(*mapping);
            const auto compressed_mapping = CompressMapping(*mapping);
            if (!compressed_mapping) {
              LOG(ERROR) << "Could not compress mapping " << file_path;
              return false;
            }
            if (!database_.RegisterFile(file_path, hash,
                                        *compressed_mapping.data,
                                        compressed_mapping.range)) {
              LOG(ERROR) << "Could not write file hash " << file_path;
              return false;
            }
            return true;
          },
          path, base_directory)) {
    return false;
  }
  return true;
}

}  // namespace pack
