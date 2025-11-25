#include "compressor.h"

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

}  // namespace pack
