#include "package.h"

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
  absl::flat_hash_map<std::string, ContentHash> hashes;
  if (!IterateDirectoryRecursively(
          [&hashes](const std::string& file_path, const UniqueFD& fd) {
            auto mapping = FileMapping::CreateReadOnly(fd);
            if (!mapping) {
              return false;
            }
            hashes[file_path] = GetMappingHash(*mapping);
            return true;
          },
          path, base_directory)) {
    return false;
  }
  return database_.WriteFileHashes(std::move(hashes));
}

}  // namespace pack
