cmake_minimum_required(VERSION 3.11.0)

include(GoogleTest)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GMock REQUIRED IMPORTED_TARGET gmock)

add_executable(Tests
    tests.cpp)

target_link_libraries(Tests
  PRIVATE
  VulkanTestLib
  PkgConfig::GMock
  gtest_main)

gtest_discover_tests(Tests)