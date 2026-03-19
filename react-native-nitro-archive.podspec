require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

Pod::Spec.new do |s|
  s.name         = "react-native-nitro-archive"
  s.module_name  = "NitroArchive"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]
  s.platforms    = { :ios => "13.0" }
  s.source       = { :git => ".git", :tag => s.version }

  # Source files — C++ implementation + vendored libraries
  s.source_files = [
    "cpp/**/*.{h,hpp,cpp,c}"
  ]

  # Exclude Android-specific crypto — iOS uses CommonCrypto via mz_crypt_apple.c
  s.exclude_files = [
    "cpp/vendor/minizip-ng/mz_crypt_openssl.c",
    "cpp/vendor/minizip-ng/mz_crypt_brg.c",
    "cpp/vendor/minizip-ng/lib/**/*"
  ]

  # Link system zlib (bzip2 is vendored) + Security framework for AES
  s.libraries = ["z"]
  s.frameworks = ["Security"]

  s.pod_target_xcconfig = {
    "CLANG_CXX_LANGUAGE_STANDARD" => "c++20",
    "OTHER_LDFLAGS" => "-lz",
    "HEADER_SEARCH_PATHS" => "$(PODS_TARGET_SRCROOT)/cpp $(PODS_TARGET_SRCROOT)/cpp/vendor/bzip2 $(PODS_TARGET_SRCROOT)/cpp/vendor/minizip-ng $(PODS_TARGET_SRCROOT)/cpp/vendor/lzma",
    "GCC_PREPROCESSOR_DEFINITIONS" => "HAVE_ZLIB ZLIB_COMPAT HAVE_WZAES HAVE_PKCRYPT MZ_ZIP_NO_SIGNING _7ZIP_ST"
  }

  # Load autolinking from nitrogen
  autolinking_script = File.join(__dir__, "nitrogen", "generated", "ios", "NitroArchive+autolinking.rb")
  if File.exist?(autolinking_script)
    load autolinking_script
    add_nitrogen_files(s)
  end

  s.dependency "NitroModules"
end
