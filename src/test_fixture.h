#pragma once

#include <absl/log/log.h>
#include <gtest/gtest.h>

#include "mapping.h"

namespace pack::testing {

class TestFixture : public ::testing::Test {
 public:
  TestFixture();

  ~TestFixture();

  void SetUp() override;

  void TearDown() override;

  std::string GetTempDirPath() const;

 private:
  std::optional<std::string> temp_dir_;
};

}  // namespace pack::testing
