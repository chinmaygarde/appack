#include "compressor.h"

#include <absl/log/log.h>
#include <zstd.h>

#include "file_mapping.h"

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

bool DecompressMapping(const uint8_t* compressed_data_size,
                       uint64_t compressed_data_length,
                       const std::filesystem::path& path,
                       const UniqueFD* base_directory) {
  const auto decompressed_content_size =
      ::ZSTD_getFrameContentSize(compressed_data_size, compressed_data_length);
  if (decompressed_content_size == ZSTD_CONTENTSIZE_UNKNOWN ||
      decompressed_content_size == ZSTD_CONTENTSIZE_ERROR) {
    LOG(ERROR) << "Compressed content size was unknown.";
    return false;
  }
  return WriteFileAtomically(
      path, base_directory, decompressed_content_size,
      [decompressed_content_size, compressed_data_size,
       compressed_data_length](uint8_t* decompressed_mapping) -> bool {
        auto actual_decompressed_size =
            ::ZSTD_decompress(decompressed_mapping, decompressed_content_size,
                              compressed_data_size, compressed_data_length);
        if (::ZSTD_isError(actual_decompressed_size)) {
          LOG(ERROR) << "Could not decompress data.";
          return false;
        }
        if (actual_decompressed_size != decompressed_content_size) {
          LOG(ERROR) << "Decompressed sizes disagree.";
          return false;
        }
        return true;
      });
}

}  // namespace pack
