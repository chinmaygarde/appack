#include "package.h"

namespace pack {

Package::Package(const std::filesystem::path& path)
    : database_(Database::Create(path)) {
  if (!database_) {
    return;
  }
  if (!database_->CreateTables()) {
    return;
  }
  is_valid_ = true;
}

Package::~Package() = default;

bool Package::IsValid() const {
  return is_valid_;
}

}  // namespace pack
