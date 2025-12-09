#pragma once

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

#include "file.h"
#include "mask.h"
#include "unique_object.h"

namespace pack {

struct Range {
  uint64_t offset = {};
  uint64_t length = {};
};

class Mapping {
 public:
  virtual uint8_t* GetData() const = 0;

  virtual uint64_t GetSize() const = 0;

  virtual ~Mapping();

  Mapping(const Mapping&) = delete;

  Mapping(Mapping&&) = delete;

  Mapping& operator=(const Mapping&) = delete;

  Mapping& operator=(Mapping&&) = delete;

 protected:
  Mapping() = default;
};

}  // namespace pack
