cmake_minimum_required(VERSION 3.22)
project(TarBz2)

set(CMAKE_CXX_STANDARD 20)

# Create the shared library
add_library(TarBz2 SHARED
  src/main/cpp/cpp-adapter.cpp
)

target_include_directories(TarBz2 PRIVATE
  ../cpp
)

# Include Nitrogen autolinking (handles all native dependencies)
include(${CMAKE_CURRENT_SOURCE_DIR}/../nitrogen/generated/android/TarBz2+autolinking.cmake)
