# Library
add_library(appack
  database.cc
  database.h
  hasher.cc
  hasher.h
  mapping.cc
  mapping.h
  mask.h
  package.cc
  package.h
  unique_object.cc
  unique_object.h
)

# Make these come from vcpkg.
find_package(zstd CONFIG REQUIRED)
find_package(blake3 CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

target_link_libraries(appack
  PUBLIC
    BLAKE3::blake3
    unofficial::sqlite3::sqlite3
    zstd::libzstd
)

# Configure Fixtures Location
get_filename_component(TEST_ASSETS_LOCATION ../assets ABSOLUTE)
configure_file(fixtures_location.h.in fixtures_location.h @ONLY)

# Unittests
add_executable(appack_tests
  unittests.cc
  ${CMAKE_CURRENT_BINARY_DIR}/fixtures_location.h
)
target_link_libraries(appack_tests
  appack
  GTest::gtest_main
)
target_include_directories(appack_tests
  PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
)
gtest_discover_tests(appack_tests)
