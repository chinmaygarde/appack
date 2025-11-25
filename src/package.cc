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

}  // namespace pack
