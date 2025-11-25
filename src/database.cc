#include "database.h"

#include <absl/log/check.h>
#include <absl/log/log.h>

namespace pack {

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

static StatementHandle CreateStatement(
    const DatabaseHandle& db,
    const std::string_view& statement_string) {
  sqlite3_stmt* statement_handle = nullptr;
  if (::sqlite3_prepare_v2(db.get(), statement_string.data(),
                           statement_string.size(), &statement_handle,
                           nullptr) != SQLITE_OK) {
    LOG(ERROR) << "Could not prepare statement.";
    return {};
  }
  return StatementHandle{statement_handle};
}

static bool CreateTables(const DatabaseHandle& handle) {
  const auto create_files_table_statement = CreateStatement(handle, R"~(
    CREATE TABLE IF NOT EXISTS appack_files(
      file_name       TEXT    PRIMARY KEY,
      content_hash    BLOB    NOT NULL
    );
    CREATE TABLE IF NOT EXISTS appack_file_contents(
      content_hash    BLOB    PRIMARY KEY,
      contents BLOB           NOT NULL
    );
  )~");
  if (::sqlite3_reset(create_files_table_statement.get()) != SQLITE_OK) {
    LOG(ERROR) << "Could not reset statement.";
    return false;
  }
  if (::sqlite3_step(create_files_table_statement.get()) != SQLITE_DONE) {
    LOG(ERROR) << "Could not step.";
    return false;
  }
  return true;
}

Database::Database(const std::filesystem::path& location) {
  sqlite3* db = nullptr;
  auto result = ::sqlite3_open(location.c_str(), &db);
  if (result != SQLITE_OK) {
    LOG(ERROR) << "Could not open database.";
    return;
  }
  handle_.reset(db);

  if (!CreateTables(handle_)) {
    return;
  }

  is_valid_ = true;
}

Database::~Database() = default;

bool Database::IsValid() const {
  return is_valid_;
}

bool Database::WriteFileHashes(
    absl::flat_hash_map<std::string, ContentHash> hashes) {
  return false;
}

}  // namespace pack
