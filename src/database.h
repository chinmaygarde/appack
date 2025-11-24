#pragma once

#include <sqlite3.h>
#include <filesystem>
#include <memory>
#include "unique_object.h"

namespace pack {

struct DatabaseHandleTraits {
  static sqlite3* InvalidValue() { return nullptr; }

  static bool IsValid(const sqlite3* value) { return value != InvalidValue(); }

  static void Free(sqlite3* handle) { sqlite3_free(handle); }
};

using DatabaseHandle = UniqueObject<sqlite3*, DatabaseHandleTraits>;

class Database final : public std::enable_shared_from_this<Database> {
 public:
  static std::shared_ptr<Database> Create(
      const std::filesystem::path& location) {
    auto db = std::shared_ptr<Database>(new Database(location));
    if (!db->IsValid()) {
      return nullptr;
    }
    return db;
  }

  ~Database();

  Database(const Database&) = delete;

  Database(Database&&) = delete;

  Database& operator=(const Database&) = delete;

  Database& operator=(Database&&) = delete;

  bool IsValid() const;

 private:
  bool is_valid_ = false;

  Database(const std::filesystem::path& location);

  DatabaseHandle handle_;
};

}  // namespace pack
