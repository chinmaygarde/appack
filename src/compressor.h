#pragma once

#include <filesystem>
#include "mapping.h"

namespace pack {

struct CompressedData {
  std::unique_ptr<Mapping> data;
  Range range;

   operator bool() const { return !!data; }
};

CompressedData CompressMapping(const Mapping& mapping);

bool DecompressMapping(const uint8_t* compressed_data_size,
                       uint64_t compressed_data_length,
                       const std::filesystem::path& path,
                       const UniqueFD* base_directory = nullptr);

}  // namespace pack
