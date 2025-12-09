#include "file_mapping.h"

#include <absl/log/log.h>

namespace pack {

class EmptyMapping final : public Mapping {
 public:
  EmptyMapping() {}

  virtual uint8_t* GetData() const { return nullptr; }

  virtual uint64_t GetSize() const { return 0; }
};

std::unique_ptr<FileMapping> FileMapping::Create(
    std::optional<int> fd,
    size_t mapping_size,
    size_t mapping_offset,
    Mask<MappingProtections> protections,
    MappingModifications mods) {
  if (mapping_size == 0) {
    LOG(ERROR) << "Cannot create a zero sized file mapping.";
    return nullptr;
  }
  int prot = 0;
  if (protections & MappingProtections::kNone) {
    prot |= PROT_NONE;
  }
  if (protections & MappingProtections::kRead) {
    prot |= PROT_READ;
  }
  if (protections & MappingProtections::kWrite) {
    prot |= PROT_WRITE;
  }
  if (protections & MappingProtections::kExecute) {
    prot |= PROT_EXEC;
  }

  int flags = 0;
  switch (mods) {
    case MappingModifications::kPrivate:
      flags |= MAP_PRIVATE;
      break;
    case MappingModifications::kShared:
      flags |= MAP_SHARED;
      break;
  }
  if (fd.has_value()) {
    flags |= MAP_FILE;
  } else {
    flags |= MAP_ANONYMOUS;
  }

  auto mapping =
      ::mmap(NULL, mapping_size, prot, flags, fd.value_or(-1), mapping_offset);
  if (mapping == MAP_FAILED) {
    PLOG(ERROR) << "mmap failed.";
    return nullptr;
  }
  auto file_mapping = std::unique_ptr<FileMapping>(
      new FileMapping(MappingHandle{mapping, mapping_size}));
  if (!file_mapping->IsValid()) {
    LOG(ERROR) << "Invalid file mapping";
    return nullptr;
  }
  return file_mapping;
}

std::unique_ptr<FileMapping> FileMapping::Create(
    const UniqueFD& file,
    size_t mapping_size,
    size_t mapping_offset,
    Mask<MappingProtections> protections,
    MappingModifications mods) {
  if (!file.is_valid()) {
    return nullptr;
  }
  return Create(file.get(), mapping_size, mapping_offset, protections, mods);
}

std::unique_ptr<Mapping> FileMapping::CreateReadOnly(const UniqueFD& fd) {
  if (!fd.is_valid()) {
    return nullptr;
  }
  const auto size = FileGetSize(fd);
  if (!size.has_value()) {
    return nullptr;
  }

  if (size.value() == 0) {
    return std::make_unique<EmptyMapping>();
  }

  return Create(fd,                             //
                size.value(),                   //
                0,                              //
                MappingProtections::kRead,      //
                MappingModifications::kPrivate  //
  );
}

std::unique_ptr<Mapping> FileMapping::CreateReadOnly(
    const std::filesystem::path& file_path,
    const UniqueFD* base_directory) {
  return CreateReadOnly(
      OpenFile(file_path, FilePermissions::kReadOnly, {}, base_directory));
}

std::unique_ptr<FileMapping> FileMapping::CreateAnonymousReadWrite(
    uint64_t size) {
  return Create(std::nullopt, size, 0u,
                MappingProtections::kRead | MappingProtections::kWrite,
                MappingModifications::kPrivate);
}

FileMapping::FileMapping(MappingHandle handle) : handle_(handle) {}

bool FileMapping::MSync() const {
  if (!IsValid()) {
    return false;
  }
  if (::msync(handle_.mapping, handle_.size, MS_SYNC) != 0) {
    PLOG(ERROR) << "Could not msync.";
    return false;
  }
  return true;
}

uint8_t* FileMapping::GetData() const {
  return reinterpret_cast<uint8_t*>(handle_.mapping);
}

uint64_t FileMapping::GetSize() const {
  return handle_.size;
}

bool FileMapping::IsValid() const {
  return !!handle_;
}

}  // namespace pack
