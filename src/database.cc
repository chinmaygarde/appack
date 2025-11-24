#include "database.h"

namespace pack {

Database::Database(const std::filesystem::path& location) {
  sqlite3* db = nullptr;
  auto result = sqlite3_open(location.c_str(), &db);
  if (result != SQLITE_OK) {
    return;
  }
  handle_.reset(db);
  is_valid_ = true;
}

Database::~Database() {}

bool Database::IsValid() const {
  return is_valid_;
}

}  // namespace pack
