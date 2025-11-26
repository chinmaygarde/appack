#include "compressor.h"

#include <absl/log/log.h>
#include <zstd.h>

namespace pack {

CompressedData CompressMapping(const Mapping& mapping) {
  const auto max_compressed_size = ZSTD_compressBound(mapping.GetSize());
  auto compressed_mapping =
      FileMapping::CreateAnonymousReadWrite(max_compressed_size);
  if (!compressed_mapping) {
    return {};
  }
  const auto compressed_size =
      ::ZSTD_compress(compressed_mapping->GetData(),  // dst
                      max_compressed_size,            // dstCapacity
                      mapping.GetData(),              // src
                      mapping.GetSize(),              // srcSize
                      ZSTD_defaultCLevel()            // compressionLevel
      );
  if (ZSTD_isError(compressed_size)) {
    return {};
  }
  return {
      .data = std::move(compressed_mapping),
      .range = Range{.offset = 0, .length = compressed_size},
  };
}

bool DecompressMapping(const uint8_t* data,
                       uint64_t length,
                       const std::filesystem::path& path,
                       const UniqueFD* base_directory) {
  const auto content_size = ::ZSTD_getFrameContentSize(data, length);
  if (content_size == ZSTD_CONTENTSIZE_UNKNOWN ||
      content_size == ZSTD_CONTENTSIZE_ERROR) {
    LOG(ERROR) << "Compressed content size was unknown.";
    return false;
  }

  auto temp_path = path;
  temp_path.concat(".appacktmp");

  auto temp_file = OpenFile(temp_path, FilePermissions::kReadWrite,
                            FileFlags::kCreateIfNecessary, base_directory);

  if (!temp_file.is_valid()) {
    LOG(ERROR) << "Could not create temp file.";
    return false;
  }
  if (!Truncate(temp_file, content_size)) {
    LOG(ERROR) << "Could not trunace file size.";
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
  auto decompressed_size = ::ZSTD_decompress(
      temp_mapping->GetData(), temp_mapping->GetSize(), data, length);
  if (::ZSTD_isError(decompressed_size)) {
    LOG(ERROR) << "Could not decompress data.";
    return false;
  }
  if (decompressed_size != content_size) {
    LOG(ERROR) << "Decompressed sizes disagree.";
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

}  // namespace pack
