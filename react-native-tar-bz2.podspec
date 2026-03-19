require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

Pod::Spec.new do |s|
  s.name         = "react-native-tar-bz2"
  s.module_name  = "TarBz2"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = "https://github.com/jash90/react-native-tar-bz2"
  s.license      = package["license"]
  s.authors      = package["author"]
  s.platforms    = { :ios => "13.0" }
  s.source       = { :git => ".git", :tag => s.version }

  # Source files — C++ implementation + vendored bzlib.h
  s.source_files = [
    "cpp/**/*.{h,hpp,cpp,c}"
  ]

  # Link system libbz2
  s.libraries = ["bz2"]

  s.pod_target_xcconfig = {
    "CLANG_CXX_LANGUAGE_STANDARD" => "c++20",
    "OTHER_LDFLAGS" => "-lbz2",
    "HEADER_SEARCH_PATHS" => "$(PODS_TARGET_SRCROOT)/cpp"
  }

  # Load autolinking from nitrogen
  autolinking_script = File.join(__dir__, "nitrogen", "generated", "ios", "TarBz2+autolinking.rb")
  if File.exist?(autolinking_script)
    load autolinking_script
    add_nitrogen_files(s)
  end

  s.dependency "NitroModules"
end
