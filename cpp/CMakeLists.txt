cmake_minimum_required(VERSION 3.22)

# This CMakeLists is for shared C++ code.
# On iOS, these files are compiled via the podspec.
# On Android, they are compiled via the android/CMakeLists.txt.

file(GLOB_RECURSE CPP_SOURCES "*.cpp")

# Expose as a header-only target for potential future use
add_library(tarbz2_cpp INTERFACE)
target_include_directories(tarbz2_cpp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
