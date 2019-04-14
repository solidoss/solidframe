#
#  Be sure to run `pod spec lint SolidFrame.podspec' to ensure this is a
#  valid spec and to remove all comments including this before submitting the spec.
#
#  To learn more about Podspec attributes see http://docs.cocoapods.org/specification.html
#  To see working Podspecs in the CocoaPods repo see https://github.com/CocoaPods/Specs/
#

Pod::Spec.new do |s|

  # ―――  Spec Metadata  ―――――――――――――――――――――――――――――――――――――――――――――――――――――――――― #
  
  s.name         = "SolidFrame"
  s.version      = "5.3.0"
  s.summary      = "Cross-platform C++ framework for asynchronous, distributed applications."

  s.description  = <<-DESC
  Cross-platform C++ framework for asynchronous, distributed applications.
                   DESC

  s.homepage     = "https://github.com/vipalade/solidframe"
  
  # ―――  Spec License  ――――――――――――――――――――――――――――――――――――――――――――――――――――――――――― #
  
  s.license      = "Boost Software License - Version 1.0 - August 17th, 2003"
  

  # ――― Author Metadata  ――――――――――――――――――――――――――――――――――――――――――――――――――――――――― #
  
  s.author             =  "Valentin Palade"
  
  # ――― Platform Specifics ――――――――――――――――――――――――――――――――――――――――――――――――――――――― #
  # support all platforms
  
  s.ios.deployment_target = '9.0'#minimum necessary to support thread local variables
  s.osx.deployment_target = '10.8'
  s.tvos.deployment_target = '9.0'
  s.watchos.deployment_target = '2.0'

  # ――― Source Location ―――――――――――――――――――――――――――――――――――――――――――――――――――――――――― #
  #
  #  Specify the location from where the source should be retrieved.
  #  Supports git, hg, bzr, svn and HTTP.
  #

  s.source       = { :git => "https://github.com/vipalade/solidframe.git", :branch => "work" }


  # ――― Source Code ―――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――― #

  s.preserve_paths = "solid", "solid/*.hpp", "solid/system", "solid/system/*.{hpp,ipp}", "solid/utility", "solid/utility/*.hpp", "solid/serialization", "solid/serialization/*.hpp", "solid/frame", "solid/frame/*.hpp", "solid/frame/aio", "solid/frame/aio/*.hpp", "solid/frame/aio/openssl", "solid/frame/aio/openssl/*.hpp", "solid/frame/mprpc", "solid/frame/mprpc/*.hpp"

  s.subspec 'system' do |sp|
    sp.name = 'system'
    sp.preserve_paths = "solid/system", "solid/system/*.hpp"
    sp.public_header_files = 'solid/system/*.hpp', 'solid/*.hpp'
    sp.source_files =  'solid/system/src/*.{cpp,hpp}', 'solid/system/*.hpp', 'solid/*.hpp'
    sp.exclude_files = 'solid/system/src/crashhandler_windows.cpp', 'solid/system/src/crashhandler_android.cpp', 'solid/system/src/stacktrace_windows.cpp', 'solid/system/src/stacktrace_windows.hpp'
    sp.xcconfig = { 'HEADER_SEARCH_PATHS' => '"$(PODS_ROOT)/SolidFrame"',  'USER_HEADER_SEARCH_PATHS' => '"$(PODS_ROOT)/SolidFrame"'}
  end

  s.subspec 'utility' do |sp|
    sp.name = 'utility'
    sp.dependency 'SolidFrame/system'
    sp.preserve_paths = "solid/utility", "solid/utility/*.hpp"
    sp.public_header_files = 'solid/utility/*.hpp'
    sp.source_files = 'solid/utility/src/*.{cpp,hpp}', 'solid/utility/*.hpp'
    sp.xcconfig = { 'HEADER_SEARCH_PATHS' => '"$(PODS_ROOT)/SolidFrame"'}
  end
  
  s.subspec 'serialization_v2' do |sp|
    sp.name = 'serialization_v2'
    sp.dependency 'SolidFrame/utility'
    sp.preserve_paths = "solid/serialization/v2", "solid/serialization/v2/*.hpp"
    sp.public_header_files = 'solid/serialization/v2/*.hpp'
    sp.source_files = 'solid/serialization/v2/src/*.{cpp,hpp}', 'solid/serialization/v2/*.hpp'
    sp.xcconfig = { 'HEADER_SEARCH_PATHS' => '"$(PODS_ROOT)/SolidFrame"'}
  end

  s.subspec 'frame' do |sp|
    sp.name = 'frame'
    sp.dependency 'SolidFrame/utility'
    sp.preserve_paths = "solid/frame", "solid/frame/*.hpp"
    sp.public_header_files = 'solid/frame/*.hpp'
    sp.source_files = 'solid/frame/src/*.{cpp,hpp}', 'solid/frame/*.hpp'
    sp.xcconfig = { 'HEADER_SEARCH_PATHS' => '"$(PODS_ROOT)/SolidFrame"'}
  end

  s.subspec 'frame_aio' do |sp|
    sp.name = 'frame_aio'
    sp.dependency 'SolidFrame/frame'
    sp.preserve_paths = "solid/frame/aio", "solid/frame/frame/aio/*.hpp"
    sp.public_header_files = 'solid/frame/aio/*.hpp'
    sp.source_files = 'solid/frame/aio/src/*.{cpp,hpp}', 'solid/frame/aio/*.hpp'
    sp.xcconfig = { 'HEADER_SEARCH_PATHS' => '"$(PODS_ROOT)/SolidFrame"'}
  end

  s.subspec 'frame_aio_openssl' do |sp|
    sp.name = 'frame_aio_openssl'
    sp.dependency 'SolidFrame/frame_aio'
    sp.dependency 'BoringSSL'
    sp.preserve_paths = "solid/frame/aio/openssl", "solid/frame/frame/aio/openssl/*.hpp"
    sp.public_header_files = 'solid/frame/aio/openssl/*.hpp'
    sp.source_files = 'solid/frame/aio/openssl/src/*.{cpp,hpp}', 'solid/frame/aio/openssl/*.hpp'
    sp.xcconfig = { 'HEADER_SEARCH_PATHS' => '"$(PODS_ROOT)/SolidFrame"'}
  end

  s.subspec 'frame_mprpc' do |sp|
    sp.name = 'frame_mprpc'
    sp.dependency 'SolidFrame/frame_aio'
    sp.preserve_paths = "solid/frame/mprpc", "solid/frame/frame/mprpc/*.hpp"
    sp.public_header_files = 'solid/frame/mprpc/*.hpp'
    sp.source_files = 'solid/frame/mprpc/src/*.{cpp,hpp}', 'solid/frame/mprpc/*.hpp'
    sp.xcconfig = { 'HEADER_SEARCH_PATHS' => '"$(PODS_ROOT)/SolidFrame"'}
  end
  
  s.header_mappings_dir = '.'

  s.prepare_command = <<-END_OF_COMMAND
    cat > "solid/solid_config.hpp" <<EOF
    #ifndef SOLID_SYSTEM_CONFIG_H
    #define SOLID_SYSTEM_CONFIG_H

      #define SOLID_USE_PTHREAD
      #define SOLID_USE_KQUEUE
      #define SOLID_USE_SAFE_STATIC
      #define SOLID_ON_DARWIN
      #define SOLID_ON_POSIX
      #define SOLID_HAS_DEBUG

      #define SOLID_USE_GCC_BSWAP

      #define SOLID_VERSION_MAJOR 5
      #define SOLID_VERSION_MINOR 2
      #define SOLID_VERSION_PATCH "0"
    #endif
    EOF
  END_OF_COMMAND

  # ――― Resources ―――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――― #
  #
  #  A list of resources included with the Pod. These are copied into the
  #  target bundle with a build phase script. Anything else will be cleaned.
  #  You can preserve files from being cleaned, please don't preserve
  #  non-essential files like tests, examples and documentation.
  #

  # s.resource  = "icon.png"
  # s.resources = "Resources/*.png"

  # s.preserve_paths = "FilesToSave", "MoreFilesToSave"


  # ――― Project Linking ―――――――――――――――――――――――――――――――――――――――――――――――――――――――――― #

  s.libraries = "c++"


  # ――― Project Settings ――――――――――――――――――――――――――――――――――――――――――――――――――――――――― #

  s.requires_arc = false
  s.xcconfig = { 'HEADER_SEARCH_PATHS' => '"$(PODS_ROOT)/SolidFrame"'}

end
