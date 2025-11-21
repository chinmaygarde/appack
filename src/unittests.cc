#include "fixtures_location.h"
#include "gtest/gtest.h"
#include "mapping.h"
#include "package.h"

namespace pack::testing {

TEST(Appack, CanCreatePackage) {
  auto mapping = FileMapping::CreateReadOnly(
      std::filesystem::path{TEST_ASSETS_LOCATION "kalimba.jpg"});
  ASSERT_NE(mapping, nullptr);
  ASSERT_EQ(mapping->GetSize(), 68061u);
  ASSERT_NE(mapping->GetData(), nullptr);
}

}  // namespace pack::testing
