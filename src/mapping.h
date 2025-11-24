#pragma once

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdint>
#include <filesystem>
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

  constexpr explicit operator bool() const { return *this == MappingHandle{}; }
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
                  const std::optional<UniqueFD>& base_directory);

std::optional<uint64_t> FileGetSize(const UniqueFD& fd);

class FileMapping final : public Mapping {
 public:
  static std::unique_ptr<FileMapping> Create(
      const UniqueFD& file,
      size_t mapping_size,
      size_t mapping_offset,
      Mask<MappingProtections> protections,
      MappingModifications mods);

  static std::unique_ptr<FileMapping> CreateReadOnly(
      const std::filesystem::path& file_path,
      const std::optional<UniqueFD>& base_directory = std::nullopt);

  uint8_t* GetData() const override;

  uint64_t GetSize() const override;

  bool IsValid() const;

 private:
  MappingHandle handle_;

  FileMapping(MappingHandle handle);
};

}  // namespace pack
