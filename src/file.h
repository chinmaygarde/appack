#pragma once

#include <filesystem>
#include <functional>
#include <optional>

#include <unistd.h>

#include "macros.h"
#include "mask.h"
#include "unique_object.h"

namespace pack {

struct UniqueFDTraits {
  static int InvalidValue() { return -1; }

  static bool IsValid(int value) { return value != InvalidValue(); }

  static void Free(int fd) { PACK_TEMP_FAILURE_RETRY(::close(fd)); }
};

using UniqueFD = UniqueObject<int, UniqueFDTraits>;

enum class FilePermissions {
  kReadOnly,
  kWriteOnly,
  kReadWrite,
};

enum class MappingProtections {
  kNone = 0 << 0,
  kRead = 1 << 0,
  kWrite = 1 << 1,
  kExecute = 1 << 2,
};
PACK_ENUM_IS_MASK(MappingProtections);

enum class MappingModifications {
  kPrivate,
  kShared,
};

enum class FileFlags {
  kCreateIfNecessary = 1 << 0,
  kTruncateToZero = 1 << 1,
  kDirectory = 1 << 2,
};
PACK_ENUM_IS_MASK(FileFlags);

UniqueFD OpenFile(const std::filesystem::path& file_path,
                  FilePermissions permissions,
                  Mask<FileFlags> flags,
                  const UniqueFD* base_directory);

bool Truncate(const UniqueFD& fd, uint64_t size);

bool Rename(const std::filesystem::path& from_path,
            const std::filesystem::path& to_path,
            const UniqueFD* from_dir_fd,
            const UniqueFD* to_dir_fd);

std::optional<uint64_t> FileGetSize(const UniqueFD& fd);

bool IsDirectory(const std::filesystem::path& file_path,
                 const UniqueFD* base_directory = nullptr);

bool IsLink(const std::filesystem::path& file_path,
            const UniqueFD* base_directory = nullptr);

bool IsRegularFile(const std::filesystem::path& file_path,
                   const UniqueFD* base_directory = nullptr);

bool MakeDirectories(const std::filesystem::path& file_path,
                     const UniqueFD* base_directory = nullptr);

bool MakeSymlink(const std::filesystem::path& from,
                 const std::string& to,
                 const UniqueFD* base_directory = nullptr);

std::optional<std::string> CreateTemporaryDirectory();

bool RemoveDirectory(const std::string& dir_name,
                     const UniqueFD* base_directory = nullptr);

bool RemovePathIfExists(const std::filesystem::path& path,
                        const UniqueFD* base_directory = nullptr);

bool RemovePath(const std::filesystem::path& path,
                const UniqueFD* base_directory = nullptr);

using FileIterator =
    std::function<bool(const std::string& file_path, const UniqueFD& fd)>;
using LinkIterator = std::function<bool(const std::string& file_path,
                                        const std::filesystem::path& path)>;
bool IterateDirectoryRecursively(FileIterator file_iterator,
                                 LinkIterator link_iterator,
                                 const std::string& dir_name,
                                 const UniqueFD* base_directory = nullptr);

using FileWriter = std::function<bool(uint8_t* mapping)>;
bool WriteFileAtomically(const std::filesystem::path& path,
                         const UniqueFD* base_directory,
                         size_t content_size,
                         FileWriter writer);

bool PathExists(const std::filesystem::path& path,
                const UniqueFD* base_directory = nullptr);

}  // namespace pack
