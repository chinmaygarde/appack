#pragma once

#include <filesystem>
#include "database.h"

namespace pack {

class Package {
 public:
  Package(const std::filesystem::path& path) : database_(path) {
    if (!database_.IsValid()) {
      return;
    }
    is_valid_ = true;
  }

  ~Package();

  Package(const Package&) = delete;

  Package(Package&&) = delete;

  Package& operator=(const Package&) = delete;

  Package& operator=(Package&&) = delete;

  bool IsValid() const { return is_valid_; }

 private:
  Database database_;
  bool is_valid_ = false;
};

}  // namespace pack
