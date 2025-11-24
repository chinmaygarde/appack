#pragma once

#include <filesystem>
#include "database.h"

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

 private:
  std::shared_ptr<Database> database_;
  bool is_valid_ = false;
};

}  // namespace pack
