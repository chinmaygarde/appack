#include "mapping.h"

namespace pack {

Mapping::~Mapping() = default;

uint8_t* FileMapping::GetData() const {
  return reinterpret_cast<uint8_t*>(handle_.mapping);
}

uint64_t FileMapping::GetSize() const {
  return handle_.size;
}

bool FileMapping::IsValid() const {
  return !!handle_;
}

UniqueFD OpenFile(const std::filesystem::path& file_path,
                  FilePermissions permissions,
                  Mask<FileFlags> flags,
                  const std::optional<UniqueFD>& base_directory) {
  int base_directory_fd = AT_FDCWD;
  if (base_directory.has_value()) {
    base_directory_fd = base_directory.value().get();
  }
  int oflag = 0;
  switch (permissions) {
    case FilePermissions::kReadOnly:
      oflag |= O_RDONLY;
      break;
    case FilePermissions::kWriteOnly:
      oflag |= O_WRONLY;
      break;
    case FilePermissions::kReadWrite:
      oflag |= O_RDWR;
      break;
  }
  if (flags & FileFlags::kCreateIfNecessary) {
    oflag |= O_CREAT;
  }
  if (flags & FileFlags::kTruncateToZero) {
    oflag |= O_TRUNC;
  }
  if (flags & FileFlags::kDirectory) {
    oflag |= O_DIRECTORY;
  }
  oflag |= O_CLOEXEC;
  int fd = PACK_TEMP_FAILURE_RETRY(
      ::openat(base_directory_fd, file_path.c_str(), oflag));
  if (fd == -1) {
    return {};
  }
  return UniqueFD{fd};
}

std::optional<uint64_t> FileGetSize(const UniqueFD& fd) {
  if (!fd.is_valid()) {
    return std::nullopt;
  }
  struct stat statbuf = {};

  if (::fstat(fd.get(), &statbuf) == -1) {
    return std::nullopt;
  }

  return statbuf.st_size;
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

  auto mapping =
      ::mmap(NULL, mapping_size, prot, flags, file.get(), mapping_offset);
  if (mapping == MAP_FAILED) {
    return nullptr;
  }
  return std::unique_ptr<FileMapping>(
      new FileMapping(MappingHandle{mapping, mapping_size}));
}

FileMapping::FileMapping(MappingHandle handle) : handle_(handle) {}

std::unique_ptr<FileMapping> FileMapping::CreateReadOnly(
    const std::filesystem::path& file_path,
    const std::optional<UniqueFD>& base_directory) {
  const auto fd =
      OpenFile(file_path, FilePermissions::kReadOnly, {}, base_directory);
  if (!fd.is_valid()) {
    return nullptr;
  }
  const auto size = FileGetSize(fd);
  if (!size.has_value()) {
    return nullptr;
  }
  return Create(fd,                             //
                size.value(),                   //
                0,                              //
                MappingProtections::kRead,      //
                MappingModifications::kPrivate  //
  );
}

}  // namespace pack
