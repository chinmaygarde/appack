#include "fixtures_location.h"
#include "gtest/gtest.h"
#include "hasher.h"
#include "mapping.h"
#include "package.h"
#include "test_fixture.h"

namespace pack::testing {

using Appack = TestFixture;

TEST_F(Appack, CanCreateMapping) {
  auto mapping = FileMapping::CreateReadOnly(
      std::filesystem::path{TEST_ASSETS_LOCATION "kalimba.jpg"});
  ASSERT_NE(mapping, nullptr);
  ASSERT_EQ(mapping->GetSize(), 68061u);
  ASSERT_NE(mapping->GetData(), nullptr);
}

TEST_F(Appack, CanParseHashFromString) {
  const auto hash = ParseFromHexString(
      "0eedeb0be9888022d3f92a799eb56d160a911a997d6b0ef0e504865da422a3fd");
  ASSERT_TRUE(hash.has_value());
  ASSERT_EQ(ToString(hash.value()),
            "0eedeb0be9888022d3f92a799eb56d160a911a997d6b0ef0e504865da422a3fd");
}

TEST_F(Appack, CanHashContents) {
  auto mapping = FileMapping::CreateReadOnly(
      std::filesystem::path{TEST_ASSETS_LOCATION "kalimba.jpg"});
  ASSERT_NE(mapping, nullptr);
  const auto h1 = GetMappingHash(*mapping);
  const auto h2 = ParseFromHexString(
      "0eedeb0be9888022d3f92a799eb56d160a911a997d6b0ef0e504865da422a3fd");
  ASSERT_TRUE(h2.has_value());
  ASSERT_EQ(h1, h2.value());
}

TEST_F(Appack, CanCreatePackageAndDecompress) {
  Package package(GetTempDirPath() + "/database.appack");
  ASSERT_TRUE(package.IsValid());
  ASSERT_TRUE(package.RegisterPath(TEST_ASSETS_LOCATION));
  const std::string install_path = GetTempDirPath() + "/decompressed";
  ASSERT_TRUE(package.InstallEmbeddedFiles(install_path));
  ASSERT_TRUE(PathExists(install_path + "/airplane.jpg"));
  ASSERT_TRUE(PathExists(install_path + "/somefolder2/airlink.jpg"));
  ASSERT_TRUE(PathExists(install_path + "/0/1/2/3/4/5/6/7/airplane.jpg"));
  ASSERT_TRUE(PathExists(install_path + "/a/b/c/d/e/f/g/airplane.jpg"));
}

TEST_F(Appack, CanDecompressOverExistingInstallation) {
  Package package(GetTempDirPath() + "/database.appack");
  ASSERT_TRUE(package.IsValid());
  ASSERT_TRUE(package.RegisterPath(TEST_ASSETS_LOCATION));
  const std::string install_path = GetTempDirPath() + "/decompressed";
  ASSERT_TRUE(package.InstallEmbeddedFiles(install_path));
  ASSERT_TRUE(package.InstallEmbeddedFiles(install_path));
  ASSERT_TRUE(package.InstallEmbeddedFiles(install_path));
}

}  // namespace pack::testing
