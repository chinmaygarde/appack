#include "package.h"

namespace pack {

Package::Package(const std::filesystem::path& path)
    : database_(Database::Create(path)) {
  if (!database_) {
    return;
  }
  is_valid_ = true;
}

bool Package::IsValid() const {
  return is_valid_;
}

}  // namespace pack
