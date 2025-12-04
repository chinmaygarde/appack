#pragma once

#include <array>
#include <optional>
#include <string_view>

#include "mapping.h"

namespace pack {

using ContentHash = std::array<uint8_t, 32>;

ContentHash GetMappingHash(const Mapping& mapping);

std::string ToString(const ContentHash& hash);

std::optional<ContentHash> ParseFromHexString(const std::string_view& str);

}  // namespace pack
