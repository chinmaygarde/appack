#include "database.h"

#include <absl/cleanup/cleanup.h>
#include <absl/log/check.h>
#include <absl/log/log.h>

namespace pack {

static StatementHandle CreateStatement(
    const DatabaseHandle& db,
    const std::string_view& statement_string) {
  sqlite3_stmt* statement_handle = nullptr;
  if (::sqlite3_prepare_v2(db.get(), statement_string.data(),
                           statement_string.size(), &statement_handle,
                           nullptr) != SQLITE_OK) {
    LOG(ERROR) << "Could not prepare statement: " << statement_string;
    return {};
  }
  return StatementHandle{statement_handle};
}

static bool CreateTables(const DatabaseHandle& handle) {
  {
    const auto stmt = CreateStatement(handle, R"~(
    CREATE TABLE IF NOT EXISTS appack_files(
      file_name       TEXT    PRIMARY KEY,
      content_hash    BLOB    NOT NULL
    );
  )~");
    if (::sqlite3_reset(stmt.get()) != SQLITE_OK) {
      LOG(ERROR) << "Could not reset statement.";
      return false;
    }
    if (::sqlite3_step(stmt.get()) != SQLITE_DONE) {
      LOG(ERROR) << "Could not step.";
      return false;
    }
  }
  {
    const auto stmt = CreateStatement(handle, R"~(
    CREATE TABLE IF NOT EXISTS appack_file_contents(
      content_hash    BLOB    PRIMARY KEY,
      contents        BLOB    NOT NULL
    );
  )~");
    if (::sqlite3_reset(stmt.get()) != SQLITE_OK) {
      LOG(ERROR) << "Could not reset statement.";
      return false;
    }
    if (::sqlite3_step(stmt.get()) != SQLITE_DONE) {
      LOG(ERROR) << "Could not step.";
      return false;
    }
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

  begin_stmt_ = CreateStatement(handle_, "BEGIN TRANSACTION;");
  commit_stmt_ = CreateStatement(handle_, "COMMIT TRANSACTION;");
  rollback_stmt_ = CreateStatement(handle_, "ROLLBACK TRANSACTION;");
  hash_stmt_ = CreateStatement(handle_, R"~(
    INSERT OR REPLACE INTO appack_files(file_name, content_hash) VALUES (?, ?);
  )~");
  content_stmt_ = CreateStatement(handle_, R"~(
    INSERT OR REPLACE INTO appack_file_contents(content_hash, contents) VALUES (?, ?);
  )~");

  if (!begin_stmt_.is_valid() || !commit_stmt_.is_valid() ||
      !rollback_stmt_.is_valid() || !hash_stmt_.is_valid() ||
      !content_stmt_.is_valid()) {
    return;
  }

  is_valid_ = true;
}

Database::~Database() = default;

bool Database::IsValid() const {
  return is_valid_;
}

bool Database::RegisterFile(std::string file_path,
                            ContentHash hash,
                            const Mapping& src_mapping,
                            const Range& src_range) {
  if (::sqlite3_step(begin_stmt_.get()) != SQLITE_DONE) {
    LOG(ERROR) << "Couldn't begin transaction.";
    return false;
  }
  absl::Cleanup rollback = [&]() {
    auto result = ::sqlite3_step(rollback_stmt_.get());
    DCHECK(result == SQLITE_DONE);
  };

  {
    // Register the content hash.
    if (::sqlite3_reset(hash_stmt_.get()) != SQLITE_OK) {
      LOG(ERROR) << "Couldn't reset statement.";
      return false;
    }
    if (::sqlite3_bind_text(hash_stmt_.get(), 1, file_path.data(),
                            file_path.size(), SQLITE_TRANSIENT) != SQLITE_OK) {
      LOG(ERROR) << "Couldn't bind key.";
      return false;
    }
    if (::sqlite3_bind_blob(hash_stmt_.get(), 2, hash.data(), hash.size(),
                            SQLITE_TRANSIENT) != SQLITE_OK) {
      LOG(ERROR) << "Couldn't bind value.";
      return false;
    }
    if (::sqlite3_step(hash_stmt_.get()) != SQLITE_DONE) {
      LOG(ERROR) << "Couldn't execute statement.";
      return false;
    }
  }

  {
    // Register the contents.
    if (::sqlite3_reset(content_stmt_.get()) != SQLITE_OK) {
      LOG(ERROR) << "Couldn't reset statement.";
      return false;
    }
    if (::sqlite3_bind_blob(content_stmt_.get(), 1, hash.data(), hash.size(),
                            SQLITE_TRANSIENT) != SQLITE_OK) {
      LOG(ERROR) << "Couldn't bind key.";
      return false;
    }
    // TODO: This needs to non-transient.
    if (::sqlite3_bind_blob(content_stmt_.get(), 2,
                            src_mapping.GetData() + src_range.offset,
                            src_range.length, SQLITE_TRANSIENT) != SQLITE_OK) {
      LOG(ERROR) << "Couldn't bind value.";
      return false;
    }
    if (::sqlite3_step(content_stmt_.get()) != SQLITE_DONE) {
      LOG(ERROR) << "Couldn't execute statement..";
      return false;
    }
  }

  std::move(rollback).Cancel();
  if (::sqlite3_step(commit_stmt_.get()) != SQLITE_DONE) {
    LOG(ERROR) << "Couldn't commit transaction.";
    return false;
  }
  return true;
}

std::optional<std::vector<std::pair<std::string, ContentHash>>>
Database::GetRegisteredFiles() const {
  auto stmt = CreateStatement(handle_, R"~(
        SELECT file_name, content_hash FROM appack_files;
      )~");
  if (!stmt.is_valid()) {
    LOG(ERROR) << "Invalid statement.";
    return std::nullopt;
  }

  if (::sqlite3_reset(stmt.get()) != SQLITE_OK) {
    LOG(ERROR) << "Could not reset statement.";
    return std::nullopt;
  }

  std::vector<std::pair<std::string, ContentHash>> results;

  while (true) {
    switch (auto step_result = ::sqlite3_step(stmt.get())) {
      case SQLITE_DONE:
        return results;
      case SQLITE_ROW: {
        auto file_name = reinterpret_cast<const char*>(
            ::sqlite3_column_text(stmt.get(), 0u));
        auto content_hash = ::sqlite3_column_blob(stmt.get(), 1u);
        auto content_hash_length = ::sqlite3_column_bytes(stmt.get(), 1u);
        ContentHash hash;
        if (content_hash_length != hash.size()) {
          LOG(ERROR) << "Content hash size was unexpected "
                     << content_hash_length;
          return std::nullopt;
        }
        ::memmove(hash.data(), content_hash, hash.size());
        results.emplace_back(std::make_pair(std::string{file_name}, hash));
      } break;
      default:
        LOG(ERROR) << "Could not step " << step_result;
        return std::nullopt;
    }
  }

  return std::nullopt;
}

bool Database::ReadContentMapping(ContentHash hash,
                                  ContentMapping callback) const {
  if (!callback) {
    LOG(ERROR) << "Callback cannot be null.";
    return false;
  }

  auto stmt = CreateStatement(handle_, R"~(
        SELECT contents FROM appack_file_contents WHERE content_hash = ?;
      )~");
  if (!stmt.is_valid()) {
    LOG(ERROR) << "Invalid statement.";
    return false;
  }

  if (::sqlite3_reset(stmt.get()) != SQLITE_OK) {
    LOG(ERROR) << "Could not reset statement.";
    return false;
  }

  if (::sqlite3_bind_blob(stmt.get(), 1, hash.data(), hash.size(),
                          SQLITE_TRANSIENT) != SQLITE_OK) {
    LOG(ERROR) << "Could not bind blob.";
    return false;
  }

  if (::sqlite3_step(stmt.get()) != SQLITE_ROW) {
    LOG(ERROR) << "Blob not found that content hash.";
    return false;
  }

  auto contents = ::sqlite3_column_blob(stmt.get(), 0);
  auto contents_length = ::sqlite3_column_bytes(stmt.get(), 0);

  return callback(reinterpret_cast<const uint8_t*>(contents), contents_length);
}

}  // namespace pack
