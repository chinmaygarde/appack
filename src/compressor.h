#pragma once

#include "mapping.h"

namespace pack {

struct CompressedData {
  std::unique_ptr<Mapping> data;
  Range range;

  constexpr operator bool() const { return !!data; }
};

CompressedData CompressMapping(const Mapping& mapping);

}  // namespace pack
