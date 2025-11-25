#include "mapping.h"

#include <dirent.h>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <string>

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
                  const UniqueFD* base_directory) {
  int base_directory_fd = AT_FDCWD;
  if (base_directory != nullptr) {
    base_directory_fd = base_directory->get();
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
    PLOG(ERROR) << "Could not open file";
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
    std::optional<int> fd,
    size_t mapping_size,
    size_t mapping_offset,
    Mask<MappingProtections> protections,
    MappingModifications mods) {
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
    return nullptr;
  }
  return std::unique_ptr<FileMapping>(
      new FileMapping(MappingHandle{mapping, mapping_size}));
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

FileMapping::FileMapping(MappingHandle handle) : handle_(handle) {}

std::unique_ptr<FileMapping> FileMapping::CreateReadOnly(const UniqueFD& fd) {
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

std::unique_ptr<FileMapping> FileMapping::CreateReadOnly(
    const std::filesystem::path& file_path,
    const UniqueFD* base_directory) {
  return CreateReadOnly(
      OpenFile(file_path, FilePermissions::kReadOnly, {}, base_directory));
}

std::optional<std::string> CreateTemporaryDirectory() {
  std::string template_string = "/tmp/appack_temp_XXXXXX";
  auto result = ::mkdtemp(template_string.data());
  if (result == NULL) {
    return std::nullopt;
  }
  return template_string;
}

struct UniqueDirTraits {
  static DIR* InvalidValue() { return NULL; }

  static bool IsValid(DIR* value) { return value != InvalidValue(); }

  static void Free(DIR* d) {
    auto result = ::closedir(d);
    DCHECK(result == 0);
  }
};

using UniqueDir = UniqueObject<DIR*, UniqueDirTraits>;

bool RemoveDirectory(const std::string& dir_name,
                     const UniqueFD* base_directory) {
  auto dir_fd =
      OpenFile(dir_name, FilePermissions::kReadOnly, {}, base_directory);
  if (!dir_fd.is_valid()) {
    return false;
  }
  UniqueDir dir(::fdopendir(dir_fd.get()));
  if (!dir.is_valid()) {
    PLOG(ERROR) << "Could not open directory";
    return false;
  }
  while (true) {
    auto dirent = ::readdir(dir.get());
    if (dirent == NULL) {
      break;
    }
    if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0) {
      continue;
    }
    const auto subdir_name = std::string{dirent->d_name};
    if (dirent->d_type == DT_DIR) {
      // Recursively remove the directory.
      if (!RemoveDirectory(subdir_name, &dir_fd)) {
        return false;
      }
    } else {
      // Unlink the file.
      bool unlink_success = ::unlinkat(dir_fd.get(),         //
                                       subdir_name.c_str(),  //
                                       0                     //
                                       ) == 0;
      if (!unlink_success) {
        PLOG(ERROR) << "Could not remove the file " << subdir_name;
        return false;
      }
    }
  }
  bool unlink_success =
      ::unlinkat(base_directory ? base_directory->get() : AT_FDCWD,  //
                 dir_name.c_str(),                                   //
                 AT_REMOVEDIR                                        //
                 ) == 0;
  if (!unlink_success) {
    PLOG(ERROR) << "Could not remove the directory";
    return false;
  }
  return true;
}

static bool IterateDirectoryRecursively(DirectoryIterator iterator,
                                        const std::string& dir_name,
                                        const UniqueFD* base_directory,
                                        std::string file_path) {
  if (!iterator) {
    return false;
  }
  auto dir_fd =
      OpenFile(dir_name, FilePermissions::kReadOnly, {}, base_directory);
  if (!dir_fd.is_valid()) {
    PLOG(ERROR) << "Could not open directory " << dir_name;
    return false;
  }
  UniqueDir dir(::fdopendir(dir_fd.get()));
  if (!dir.is_valid()) {
    PLOG(ERROR) << "Could not open directory";
    return false;
  }
  while (true) {
    auto dirent = ::readdir(dir.get());
    if (dirent == NULL) {
      break;
    }
    if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0) {
      continue;
    }
    const auto subfile_name = std::string{dirent->d_name};
    const auto subfile_path =
        file_path.empty() ? subfile_name : file_path + "/" + subfile_name;
    if (dirent->d_type == DT_DIR) {
      // Recursively iterator into the directory.
      if (!IterateDirectoryRecursively(iterator, subfile_name, &dir_fd,
                                       subfile_path)) {
        return false;
      }
    } else if (dirent->d_type) {
      // Invoke the iterator.
      auto subfile_fd =
          OpenFile(subfile_name, FilePermissions::kReadOnly, {}, &dir_fd);
      if (!subfile_fd.is_valid()) {
        PLOG(ERROR) << "Could not open file " << subfile_name;
        return false;
      }
      if (!iterator(subfile_path, subfile_fd)) {
        return false;
      }
    }
  }
  return true;
}

bool IterateDirectoryRecursively(DirectoryIterator iterator,
                                 const std::string& dir_name,
                                 const UniqueFD* base_directory) {
  return IterateDirectoryRecursively(iterator, dir_name, base_directory, "");
}

std::unique_ptr<FileMapping> FileMapping::CreateAnonymousReadWrite(
    uint64_t size) {
  return Create(std::nullopt, size, 0u,
                MappingProtections::kRead | MappingProtections::kWrite,
                MappingModifications::kPrivate);
}

}  // namespace pack
