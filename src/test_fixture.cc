#include "test_fixture.h"

namespace pack::testing {

TestFixture::TestFixture() : temp_dir_(CreateTemporaryDirectory()) {}

TestFixture::~TestFixture() {}

void TestFixture::SetUp() {
  ASSERT_TRUE(temp_dir_.has_value());
}

void TestFixture::TearDown() {
  ASSERT_TRUE(RemoveDirectory(temp_dir_.value()));
}

std::string TestFixture::GetTempDirPath() const {
  return temp_dir_.value_or("");
}

}  // namespace pack::testing
