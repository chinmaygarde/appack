#pragma once

#include <absl/container/flat_hash_map.h>
#include <absl/log/check.h>
#include <sqlite3.h>
#include <filesystem>
#include <functional>
#include <tuple>
#include <variant>

#include "hasher.h"
#include "unique_object.h"

namespace pack {

struct DatabaseHandleTraits {
  static sqlite3* InvalidValue() { return nullptr; }

  static bool IsValid(const sqlite3* value) { return value != InvalidValue(); }

  static void Free(sqlite3* handle) { ::sqlite3_free(handle); }
};

using DatabaseHandle = UniqueObject<sqlite3*, DatabaseHandleTraits>;

struct StatementHandleTraits {
  static sqlite3_stmt* InvalidValue() { return nullptr; }

  static bool IsValid(const sqlite3_stmt* value) {
    return value != InvalidValue();
  }

  static void Free(sqlite3_stmt* handle) {
    const auto result = sqlite3_finalize(handle);
    DCHECK(result == SQLITE_OK);
  }
};

using StatementHandle = UniqueObject<sqlite3_stmt*, StatementHandleTraits>;

class Database final {
 public:
  Database(const std::filesystem::path& location);

  ~Database();

  Database(const Database&) = delete;

  Database(Database&&) = delete;

  Database& operator=(const Database&) = delete;

  Database& operator=(Database&&) = delete;

  bool IsValid() const;

  bool RegisterFile(std::string file_path,
                    ContentHash hash,
                    const Mapping& mapping,
                    const Range& src_range);

  bool RegisterSymlink(std::string file_path, std::string symlink_path);

  struct RegisteredFile {
    // Either the hash of the file contents of the symlink path.
    std::variant<ContentHash, std::string> contents;
  };

  std::optional<std::vector<std::pair<std::string, RegisteredFile>>>
  GetRegisteredFiles() const;

  using ContentMapping =
      std::function<bool(const uint8_t* data, uint64_t length)>;
  bool ReadContentMapping(ContentHash hash, ContentMapping callback) const;

 private:
  DatabaseHandle handle_;
  StatementHandle begin_stmt_;
  StatementHandle commit_stmt_;
  StatementHandle rollback_stmt_;
  StatementHandle hash_stmt_;
  StatementHandle content_stmt_;
  StatementHandle insert_symlink_stmt_;
  StatementHandle read_files_stmt_;
  bool is_valid_ = false;
};

}  // namespace pack
