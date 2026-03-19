cmake_minimum_required(VERSION 3.22)
project(NitroArchive)

set(CMAKE_CXX_STANDARD 20)

# Build shared C++ code (bzip2 + minizip-ng + lzma + compression + tar + zip + sevenz)
add_subdirectory(../cpp archive_cpp_build)

# Create the shared library (HybridArchive compiled here with Nitro includes)
add_library(NitroArchive SHARED
  src/main/cpp/cpp-adapter.cpp
  ../cpp/HybridArchive.cpp
)

target_include_directories(NitroArchive PRIVATE
  ../cpp
  ../cpp/vendor/bzip2
  ../cpp/vendor/minizip-ng
  ../cpp/vendor/minizip-ng/lib/brg
  ../cpp/vendor/lzma
)

target_link_libraries(NitroArchive
  archive_cpp
  z
  log
)

# Include Nitrogen autolinking
include(${CMAKE_CURRENT_SOURCE_DIR}/../nitrogen/generated/android/NitroArchive+autolinking.cmake)
