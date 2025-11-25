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

  bool RegisterFilesInDirectory(const std::filesystem::path& path,
                                const UniqueFD* base_directory = nullptr);

  bool WriteRegisteredFilesToDirectory(
      const std::filesystem::path& path) const {
    auto files = database_.GetRegisteredFiles();
    if (!files.has_value()) {
      return false;
    }
    for (const auto& file : files.value()) {
      LOG(ERROR) << file.first;
    }
    return true;
  }

 private:
  Database database_;
  bool is_valid_ = false;
};

}  // namespace pack
