#include "mapping.h"

#include <dirent.h>
#include <sys/stat.h>
#include <filesystem>

#include <absl/cleanup/cleanup.h>
#include <absl/log/check.h>
#include <absl/log/log.h>
#include <sys/mman.h>
#include <string>

namespace pack {

Mapping::~Mapping() = default;

class EmptyMapping final : public Mapping {
 public:
  EmptyMapping() {}

  virtual uint8_t* GetData() const { return nullptr; }

  virtual uint64_t GetSize() const { return 0; }
};

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
  int omode = 0;
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
    omode |= S_IRWXU | S_IRWXG | S_IRWXO;
  }
  if (flags & FileFlags::kTruncateToZero) {
    oflag |= O_TRUNC;
  }
  if (flags & FileFlags::kDirectory) {
    oflag |= O_DIRECTORY;
  }
  oflag |= O_CLOEXEC;
  int fd = PACK_TEMP_FAILURE_RETRY(
      ::openat(base_directory_fd, file_path.c_str(), oflag, omode));
  if (fd == -1) {
    PLOG(ERROR) << "Could not open file " << file_path;
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

FileMapping::FileMapping(MappingHandle handle) : handle_(handle) {}

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
      // Remove the file.
      if (!RemovePath(subdir_name, &dir_fd)) {
        return false;
      }
    }
  }
  if (!RemovePath(dir_name, base_directory)) {
    return false;
  }
  return true;
}

static std::optional<std::string> ReadLink(const std::string& link_name,
                                           const UniqueFD* base_directory) {
  const auto dirfd =
      base_directory == nullptr ? AT_FDCWD : base_directory->get();
  struct stat statbuf = {};
  if (::fstatat(dirfd, link_name.c_str(), &statbuf, AT_SYMLINK_NOFOLLOW) != 0) {
    PLOG(ERROR) << "Could not stat link path.";
    return std::nullopt;
  }

  if (!S_ISLNK(statbuf.st_mode)) {
    LOG(ERROR) << "Path is not a link: " << link_name << ". Mode: " << std::hex
               << statbuf.st_mode;
    return std::nullopt;
  }

  char* link_path_bytes =
      reinterpret_cast<char*>(::calloc(statbuf.st_size, sizeof(char)));
  if (link_path_bytes == nullptr) {
    LOG(ERROR) << "Could not allocate bytes for link path.";
    return std::nullopt;
  }
  absl::Cleanup free_link_path_bytes = [link_path_bytes]() {
    ::free(link_path_bytes);
  };
  const auto bytes_read =
      readlinkat(dirfd, link_name.data(), link_path_bytes, statbuf.st_size);
  if (bytes_read != statbuf.st_size) {
    LOG(ERROR) << std::format("Unexpected size of link. Expected {}, got {}.",
                              statbuf.st_size, bytes_read);
    return std::nullopt;
  }
  return std::string{link_path_bytes, static_cast<size_t>(statbuf.st_size)};
}

static bool IterateDirectoryRecursively(FileIterator file_iterator,
                                        LinkIterator link_iterator,
                                        const std::string& dir_name,
                                        const UniqueFD* base_directory,
                                        std::string file_path) {
  if (!file_iterator || !link_iterator) {
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
    switch (dirent->d_type) {
      case DT_DIR:
        // Recursively iterator into the directory.
        if (!IterateDirectoryRecursively(file_iterator, link_iterator,
                                         subfile_name, &dir_fd, subfile_path)) {
          return false;
        }
        break;
      case DT_REG:
        // Open the file and invoke the iterator.
        {
          auto subfile_fd =
              OpenFile(subfile_name, FilePermissions::kReadOnly, {}, &dir_fd);
          if (!subfile_fd.is_valid()) {
            PLOG(ERROR) << "Could not open file " << subfile_name;
            return false;
          }
          if (!file_iterator(subfile_path, subfile_fd)) {
            return false;
          }
        }
        break;
      case DT_LNK: {
        auto link_path = ReadLink(subfile_name, &dir_fd);
        if (!link_path.has_value()) {
          LOG(ERROR) << "Could not read link path.";
          return false;
        }
        if (!link_iterator(subfile_path, link_path.value())) {
          return false;
        }
      } break;
      default:
        break;
    }
  }
  return true;
}

bool IterateDirectoryRecursively(FileIterator file_iterator,
                                 LinkIterator link_iterator,
                                 const std::string& dir_name,
                                 const UniqueFD* base_directory) {
  return IterateDirectoryRecursively(file_iterator, link_iterator, dir_name,
                                     base_directory, "");
}

std::unique_ptr<FileMapping> FileMapping::CreateAnonymousReadWrite(
    uint64_t size) {
  return Create(std::nullopt, size, 0u,
                MappingProtections::kRead | MappingProtections::kWrite,
                MappingModifications::kPrivate);
}

static bool MakeDirectory(const std::filesystem::path& file_path,
                          const UniqueFD* base_directory) {
  if (::mkdirat(base_directory == nullptr ? AT_FDCWD : base_directory->get(),
                file_path.c_str(), S_IRWXU) != 0) {
    PLOG(ERROR) << "Could not make directory " << file_path;
    return false;
  }
  return true;
}

bool MakeDirectories(const std::filesystem::path& file_path,
                     const UniqueFD* base_directory) {
  // Fast path.
  if (IsDirectory(file_path, base_directory)) {
    return true;
  }
  std::filesystem::path path;
  for (const auto& p : file_path) {
    if (path.empty()) {
      path += p;
    } else {
      path /= p;
    }
    if (!IsDirectory(path)) {
      if (!MakeDirectory(path, base_directory)) {
        return false;
      }
    }
  }
  return true;
}

bool Truncate(const UniqueFD& fd, uint64_t size) {
  if (::ftruncate(fd.get(), size) != 0) {
    PLOG(ERROR) << "Could not truncate file to size: " << size;
    return false;
  }
  return true;
}

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

bool Rename(const std::filesystem::path& from_path,
            const std::filesystem::path& to_path,
            const UniqueFD* from_dir_fd,
            const UniqueFD* to_dir_fd) {
  if (::renameat(from_dir_fd == nullptr ? AT_FDCWD : from_dir_fd->get(),
                 from_path.c_str(),
                 to_dir_fd == nullptr ? AT_FDCWD : to_dir_fd->get(),
                 to_path.c_str()) != 0) {
    PLOG(ERROR) << "Could not perform a file rename.";
    return false;
  }
  return true;
}

bool WriteFileAtomically(const std::filesystem::path& path,
                         const UniqueFD* base_directory,
                         size_t content_size,
                         FileWriter writer) {
  if (!writer) {
    return false;
  }

  // If the content size is zero, we won't be able to create the temp file with
  // a mapping. Just create and truncate the target file directly.
  if (content_size == 0) {
    auto target_file =
        OpenFile(path, FilePermissions::kReadWrite,
                 FileFlags::kCreateIfNecessary | FileFlags::kTruncateToZero,
                 base_directory);
    if (!target_file.is_valid()) {
      LOG(ERROR) << "Could not create target file.";
      return false;
    }
    return true;
  }

  auto temp_path = path;
  temp_path.concat(".appacktmp");

  auto temp_file =
      OpenFile(temp_path, FilePermissions::kReadWrite,
               FileFlags::kCreateIfNecessary | FileFlags::kTruncateToZero,
               base_directory);

  if (!temp_file.is_valid()) {
    LOG(ERROR) << "Could not create temp file.";
    return false;
  }
  if (!Truncate(temp_file, content_size)) {
    LOG(ERROR) << "Could not truncate file size.";
    return false;
  }
  auto temp_mapping = FileMapping::Create(
      temp_file, content_size, 0u,
      MappingProtections::kRead | MappingProtections::kWrite,
      MappingModifications::kShared);
  if (!temp_mapping) {
    LOG(ERROR) << "Could not create temp mapping.";
    return false;
  }
  if (!writer(temp_mapping->GetData())) {
    LOG(ERROR) << "Writer could not write to the temporary file.";
    return false;
  }
  if (!temp_mapping->MSync()) {
    return false;
  }
  if (!Rename(temp_path, path, base_directory, base_directory)) {
    return false;
  }
  return true;
}

bool PathExists(const std::filesystem::path& path,
                const UniqueFD* base_directory) {
  if (::faccessat(base_directory == nullptr ? AT_FDCWD : base_directory->get(),
                  path.c_str(), F_OK, AT_SYMLINK_NOFOLLOW) != 0) {
    return false;
  }
  return true;
}

enum class FileType {
  kRegularFile,
  kDirectory,
  kSymbolicLink,
};

static bool IsFileOfType(const std::filesystem::path& file_path,
                         const UniqueFD* base_directory,
                         FileType type) {
  struct stat statbuf = {};
  if (::fstatat(base_directory == nullptr ? AT_FDCWD : base_directory->get(),
                file_path.c_str(), &statbuf, 0) != 0) {
    return false;
  }
  switch (type) {
    case FileType::kRegularFile:
      return S_ISREG(statbuf.st_mode);
    case FileType::kDirectory:
      return S_ISDIR(statbuf.st_mode);
    case FileType::kSymbolicLink:
      return S_ISLNK(statbuf.st_mode);
  }
  return false;
}

bool IsDirectory(const std::filesystem::path& file_path,
                 const UniqueFD* base_directory) {
  return IsFileOfType(file_path, base_directory, FileType::kDirectory);
}

bool IsLink(const std::filesystem::path& file_path,
            const UniqueFD* base_directory) {
  return IsFileOfType(file_path, base_directory, FileType::kSymbolicLink);
}

bool IsRegularFile(const std::filesystem::path& file_path,
                   const UniqueFD* base_directory) {
  return IsFileOfType(file_path, base_directory, FileType::kRegularFile);
}

bool MakeSymlink(const std::filesystem::path& from,
                 const std::string& to,
                 const UniqueFD* base_directory) {
  const auto base_fd =
      base_directory == nullptr ? AT_FDCWD : base_directory->get();
  if (!RemovePathIfExists(from, base_directory)) {
    LOG(ERROR) << "Could not remove path.";
    return false;
  }
  if (::symlinkat(to.c_str(), base_fd, from.c_str()) != 0) {
    PLOG(ERROR) << "Could not create symlink " << from << " to " << to << ":";
    return false;
  }
  return true;
}

bool RemovePathIfExists(const std::filesystem::path& path,
                        const UniqueFD* base_directory) {
  if (!PathExists(path, base_directory)) {
    // Path doesn't exist. Nothing to do.
    return true;
  }
  return RemovePath(path, base_directory);
}

bool RemovePath(const std::filesystem::path& path,
                const UniqueFD* base_directory) {
  const auto base_dir_fd =
      base_directory == nullptr ? AT_FDCWD : base_directory->get();

  struct stat statbuf = {};
  if (::fstatat(base_dir_fd, path.c_str(), &statbuf, AT_SYMLINK_NOFOLLOW) !=
      0) {
    PLOG(ERROR) << "Could not determine file type: ";
    return true;
  }

  int unlink_flags = 0;
  if (S_ISDIR(statbuf.st_mode)) {
    unlink_flags |= AT_REMOVEDIR;
  }

  if (::unlinkat(base_dir_fd, path.c_str(), unlink_flags) != 0) {
    PLOG(ERROR) << "Could not unlink: ";
    return false;
  }
  return true;
}

}  // namespace pack
