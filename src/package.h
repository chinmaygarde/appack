#pragma once

#include <filesystem>

#include <absl/container/flat_hash_map.h>
#include <absl/log/log.h>
#include "database.h"
#include "hasher.h"
#include "mapping.h"

namespace pack {

class Package {
 public:
  Package(const std::filesystem::path& path);

  ~Package();

  Package(const Package&) = delete;

  Package(Package&&) = delete;

  Package& operator=(const Package&) = delete;

  Package& operator=(Package&&) = delete;

  bool IsValid() const;

  bool RegisterPath(const std::filesystem::path& path,
                    const UniqueFD* base_directory = nullptr);

  bool RegisterPaths(std::vector<std::filesystem::path> paths,
                     const UniqueFD* base_directory = nullptr);

  bool WriteRegisteredFilesToDirectory(
      const std::filesystem::path& root_path,
      const UniqueFD* base_directory = nullptr) const;

 private:
  Database database_;
  bool is_valid_ = false;

  bool RegisterNamedPath(const std::string& file_path, const UniqueFD& fd);

  bool RegisterDirectory(const std::filesystem::path& path,
                         const UniqueFD* base_directory = nullptr);

  bool RegisterFile(const std::filesystem::path& path,
                    const UniqueFD* base_directory = nullptr);
};

}  // namespace pack
