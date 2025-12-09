#pragma once

#include <filesystem>
#include <memory>

#include <sys/mman.h>

#include "mapping.h"
#include "unique_object.h"

namespace pack {

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
  UniqueMappingHandle handle_;

  FileMapping(MappingHandle handle);

  static std::unique_ptr<FileMapping> Create(
      std::optional<int> file,
      size_t mapping_size,
      size_t mapping_offset,
      Mask<MappingProtections> protections,
      MappingModifications mods);
};

}  // namespace pack
