# Library
add_library(appack
  project.h
  project.cc
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
