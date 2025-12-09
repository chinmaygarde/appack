#include "package.h"

#include <absl/log/log.h>

#include "compressor.h"
#include "file_mapping.h"
#include "hasher.h"

namespace pack {

Package::Package(const std::filesystem::path& path) : database_(path) {
  if (!database_.IsValid()) {
    return;
  }
  is_valid_ = true;
}

Package::~Package() = default;

bool Package::IsValid() const {
  return is_valid_;
}

bool Package::RegisterDirectory(const std::filesystem::path& path,
                                const UniqueFD* base_directory) {
  if (!IterateDirectoryRecursively(
          std::bind(&Package::RegisterNamedFilePath, this,
                    std::placeholders::_1, std::placeholders::_2),
          std::bind(&Package::RegisterNamedFileLink, this,
                    std::placeholders::_1, std::placeholders::_2),
          path, base_directory)) {
    return false;
  }
  return true;
}

bool Package::InstallEmbeddedFiles(const std::filesystem::path& root_path,
                                   const UniqueFD* base_directory) const {
  auto files = database_.GetRegisteredFiles();
  if (!files.has_value()) {
    return false;
  }
  for (const auto& file : files.value()) {
    // Create the intermediate directories if they don't already exist.
    const auto path = root_path / std::filesystem::path{file.first};
    if (path.has_parent_path()) {
      if (!MakeDirectories(path.parent_path(), base_directory)) {
        LOG(ERROR) << "Could not make make directories: " << path.parent_path();
        return false;
      }
    }

    Database::ContentMapping content_mapping_callback =
        [path, base_directory](const uint8_t* compressed_data,
                               uint64_t compressed_data_length) {
          return DecompressMapping(compressed_data,         //
                                   compressed_data_length,  //
                                   path,                    //
                                   base_directory           //
          );
        };

    if (std::holds_alternative<ContentHash>(file.second.contents)) {
      if (!database_.ReadContentMapping(
              std::get<ContentHash>(file.second.contents),  // content hash
              content_mapping_callback  // content mapping callback
              )) {
        LOG(ERROR) << "Could not write decompressed mapping to location.";
        return false;
      }
    } else if (std::holds_alternative<std::string>(file.second.contents)) {
      const auto& symlink_path = std::get<std::string>(file.second.contents);
      if (!MakeSymlink(path, symlink_path, base_directory)) {
        LOG(ERROR) << "Could not create symlink.";
        return false;
      }
    }
  }
  return true;
}

bool Package::RegisterPath(const std::filesystem::path& path,
                           const UniqueFD* base_directory) {
  if (IsDirectory(path, base_directory)) {
    return RegisterDirectory(path, base_directory);
  }
  return RegisterFile(path, base_directory);
}

bool Package::RegisterFile(const std::filesystem::path& path,
                           const UniqueFD* base_directory) {
  if (!path.has_filename()) {
    LOG(ERROR) << "Path has no filename.";
    return false;
  }
  return RegisterNamedFilePath(
      path.filename().c_str(),
      OpenFile(path, FilePermissions::kReadOnly, {}, base_directory));
}

bool Package::RegisterNamedFilePath(const std::string& file_path,
                                    const UniqueFD& fd) {
  const auto mapping = FileMapping::CreateReadOnly(fd);
  if (!mapping) {
    LOG(ERROR) << "Could not create file mapping for " << file_path;
    return false;
  }
  const auto hash = GetMappingHash(*mapping);
  const auto compressed_mapping = CompressMapping(*mapping);
  if (!compressed_mapping) {
    LOG(ERROR) << "Could not compress mapping " << file_path;
    return false;
  }
  if (!database_.RegisterFile(file_path, hash, *compressed_mapping.data,
                              compressed_mapping.range)) {
    LOG(ERROR) << "Could not write file hash " << file_path;
    return false;
  }
  return true;
}

bool Package::RegisterPaths(std::vector<std::filesystem::path> paths,
                            const UniqueFD* base_directory) {
  for (const auto& path : paths) {
    if (!PathExists(path, base_directory)) {
      LOG(ERROR) << "Path does not exist: " << path;
      return false;
    }
  }

  for (const auto& path : paths) {
    if (!RegisterPath(path, base_directory)) {
      LOG(ERROR) << "Could not register path: " << path;
      return false;
    }
  }

  return true;
}

std::optional<std::vector<std::pair<std::string, std::string>>>
Package::ListFiles() const {
  auto list = database_.GetRegisteredFiles();
  if (!list.has_value()) {
    return std::nullopt;
  }
  std::vector<std::pair<std::string, std::string>> results;
  results.reserve(list.value().size());
  for (const auto& item : list.value()) {
    if (std::holds_alternative<ContentHash>(item.second.contents)) {
      results.emplace_back(std::make_pair(
          item.first, ToString(std::get<ContentHash>(item.second.contents))));
    }
  }
  return results;
}

bool Package::RegisterNamedFileLink(const std::string& file_path,
                                    const std::filesystem::path& path) {
  if (!database_.RegisterSymlink(file_path, path.string())) {
    LOG(ERROR) << "Could not register symlink: " << file_path;
    return false;
  }
  return true;
}

}  // namespace pack
