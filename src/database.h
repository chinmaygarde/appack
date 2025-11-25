#pragma once

#include <absl/container/flat_hash_map.h>
#include <sqlite3.h>
#include <filesystem>

#include "hasher.h"
#include "unique_object.h"

namespace pack {

struct DatabaseHandleTraits {
  static sqlite3* InvalidValue() { return nullptr; }

  static bool IsValid(const sqlite3* value) { return value != InvalidValue(); }

  static void Free(sqlite3* handle) { ::sqlite3_free(handle); }
};

using DatabaseHandle = UniqueObject<sqlite3*, DatabaseHandleTraits>;

class Database final {
 public:
  Database(const std::filesystem::path& location);

  ~Database();

  Database(const Database&) = delete;

  Database(Database&&) = delete;

  Database& operator=(const Database&) = delete;

  Database& operator=(Database&&) = delete;

  bool IsValid() const;

  bool WriteFileHashes(absl::flat_hash_map<std::string, ContentHash> hashes);

 private:
  DatabaseHandle handle_;
  bool is_valid_ = false;
};

}  // namespace pack
