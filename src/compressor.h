#pragma once

#include <filesystem>
#include "mapping.h"

namespace pack {

struct CompressedData {
  std::unique_ptr<Mapping> data;
  Range range;

  constexpr operator bool() const { return !!data; }
};

CompressedData CompressMapping(const Mapping& mapping);

bool DecompressMapping(const uint8_t* data,
                       uint64_t length,
                       const std::filesystem::path& path,
                       const UniqueFD* base_directory = nullptr);

}  // namespace pack
