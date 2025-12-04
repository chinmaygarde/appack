#pragma once

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include "mask.h"
#include "unique_object.h"

namespace pack {

#define PACK_TEMP_FAILURE_RETRY(expression)          \
  ({                                                 \
    intptr_t __result;                               \
    do {                                             \
      __result = (expression);                       \
    } while ((__result == -1L) && (errno == EINTR)); \
    __result;                                        \
  })

struct Range {
  uint64_t offset = 0u;
  uint64_t length = 0u;
};

class Mapping {
 public:
  virtual uint8_t* GetData() const = 0;

  virtual uint64_t GetSize() const = 0;

  virtual ~Mapping();

  Mapping(const Mapping&) = delete;

  Mapping(Mapping&&) = delete;

  Mapping& operator=(const Mapping&) = delete;

  Mapping& operator=(Mapping&&) = delete;

 protected:
  Mapping() = default;
};

struct UniqueFDTraits {
  static int InvalidValue() { return -1; }

  static bool IsValid(int value) { return value != InvalidValue(); }

  static void Free(int fd) { PACK_TEMP_FAILURE_RETRY(::close(fd)); }
};

using UniqueFD = UniqueObject<int, UniqueFDTraits>;

struct MappingHandle {
  void* mapping = MAP_FAILED;
  size_t size = 0;

  constexpr bool operator==(const MappingHandle& handle) const = default;

  explicit operator bool() const { return mapping != MAP_FAILED; }
};

struct UniqueMappingHandleTraits {
  static MappingHandle InvalidValue() { return {}; }

  static bool IsValid(const MappingHandle& value) {
    return value != InvalidValue();
  }

  static void Free(const MappingHandle& handle) {
    ::munmap(handle.mapping, handle.size);
  }
};

using UniqueMappingHandle =
    UniqueObject<MappingHandle, UniqueMappingHandleTraits>;

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

bool MakeDirectories(const std::filesystem::path& file_path,
                     const UniqueFD* base_directory = nullptr);

std::optional<std::string> CreateTemporaryDirectory();

bool RemoveDirectory(const std::string& dir_name,
                     const UniqueFD* base_directory = nullptr);

using DirectoryIterator =
    std::function<bool(const std::string& file_path, const UniqueFD& fd)>;
bool IterateDirectoryRecursively(DirectoryIterator iterator,
                                 const std::string& dir_name,
                                 const UniqueFD* base_directory = nullptr);

using FileWriter = std::function<bool(uint8_t* mapping)>;
bool WriteFileAtomically(const std::filesystem::path& path,
                         const UniqueFD* base_directory,
                         size_t content_size,
                         FileWriter writer);

bool PathExists(const std::filesystem::path& path,
                const UniqueFD* base_directory = nullptr);

class FileMapping final : public Mapping {
 public:
  static std::unique_ptr<FileMapping> Create(
      const UniqueFD& file,
      size_t mapping_size,
      size_t mapping_offset,
      Mask<MappingProtections> protections,
      MappingModifications mods);

  static std::unique_ptr<Mapping> CreateReadOnly(
      const std::filesystem::path& file_path,
      const UniqueFD* base_directory = nullptr);

  static std::unique_ptr<Mapping> CreateReadOnly(const UniqueFD& file);

  static std::unique_ptr<FileMapping> CreateAnonymousReadWrite(uint64_t size);

  uint8_t* GetData() const override;

  uint64_t GetSize() const override;

  bool IsValid() const;

  bool MSync() const;

 private:
  MappingHandle handle_;

  FileMapping(MappingHandle handle);

  static std::unique_ptr<FileMapping> Create(
      std::optional<int> file,
      size_t mapping_size,
      size_t mapping_offset,
      Mask<MappingProtections> protections,
      MappingModifications mods);
};

}  // namespace pack
