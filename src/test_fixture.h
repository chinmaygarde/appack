#pragma once

#include <absl/log/log.h>
#include <gtest/gtest.h>

#include "mapping.h"

namespace pack::testing {

class TestFixture : public ::testing::Test {
 public:
  TestFixture() : temp_dir_(CreateTemporaryDirectory()) {}

  ~TestFixture() {}

  void SetUp() override { ASSERT_TRUE(temp_dir_.has_value()); }

  void TearDown() override { ASSERT_TRUE(RemoveDirectory(temp_dir_.value())); }

 private:
  std::optional<std::string> temp_dir_;
};

}  // namespace pack::testing
