cmake_minimum_required(VERSION 3.22)

# Shared C++ code for react-native-nitro-archive
# On iOS, compiled via podspec. On Android, via android/CMakeLists.txt.

# Vendored bzip2 sources
file(GLOB BZIP2_SOURCES "vendor/bzip2/*.c")

# Vendored minizip-ng sources (ZIP support)
# Exclude platform-specific crypto files — add correct one based on platform
file(GLOB MINIZIP_SOURCES "vendor/minizip-ng/*.c")
# Remove all platform-specific crypto implementations from glob
list(FILTER MINIZIP_SOURCES EXCLUDE REGEX "mz_crypt_apple\\.c$")
list(FILTER MINIZIP_SOURCES EXCLUDE REGEX "mz_crypt_openssl\\.c$")
list(FILTER MINIZIP_SOURCES EXCLUDE REGEX "mz_crypt_brg\\.c$")

set(MINIZIP_EXTRA_INCLUDES "")

if(APPLE)
  list(APPEND MINIZIP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/vendor/minizip-ng/mz_crypt_apple.c")
else()
  # Android — use Brian Gladman's pure C crypto (no OpenSSL dependency)
  list(APPEND MINIZIP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/vendor/minizip-ng/mz_crypt_brg.c")
  file(GLOB BRG_SOURCES "vendor/minizip-ng/lib/brg/*.c")
  list(APPEND MINIZIP_SOURCES ${BRG_SOURCES})
  set(MINIZIP_EXTRA_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/vendor/minizip-ng/lib/brg")
  set_source_files_properties(${BRG_SOURCES} PROPERTIES COMPILE_FLAGS "-w")
endif()

set_source_files_properties(${MINIZIP_SOURCES} PROPERTIES
  COMPILE_FLAGS "-w -DHAVE_ZLIB -DHAVE_WZAES -DHAVE_PKCRYPT -DMZ_ZIP_NO_SIGNING")

# Vendored LZMA SDK sources (7z support)
file(GLOB LZMA_SOURCES "vendor/lzma/*.c")
set_source_files_properties(${LZMA_SOURCES} PROPERTIES
  COMPILE_FLAGS "-w -D_7ZIP_ST")

# Compression streams
set(COMPRESSION_SOURCES
  compression/BzipStream.cpp
  compression/GzipStream.cpp
)

# Tar handling
set(TAR_SOURCES
  tar/TarReader.cpp
  tar/TarWriter.cpp
)

# ZIP handler
set(ZIP_SOURCES
  zip/ZipHandler.cpp
)

# 7z handler
set(SEVENZ_SOURCES
  sevenz/SevenZipHandler.cpp
)

# Main HybridObject
set(ARCHIVE_SOURCES
  HybridArchive.cpp
)

add_library(archive_cpp STATIC
  ${BZIP2_SOURCES}
  ${MINIZIP_SOURCES}
  ${LZMA_SOURCES}
  ${COMPRESSION_SOURCES}
  ${TAR_SOURCES}
  ${ZIP_SOURCES}
  ${SEVENZ_SOURCES}
  ${ARCHIVE_SOURCES}
)

target_include_directories(archive_cpp PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/bzip2
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/minizip-ng
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lzma
  ${MINIZIP_EXTRA_INCLUDES}
)

# Suppress warnings for vendored bzip2
set_source_files_properties(${BZIP2_SOURCES} PROPERTIES COMPILE_FLAGS "-w")

target_compile_features(archive_cpp PUBLIC cxx_std_20)
