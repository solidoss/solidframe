cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
#-----------------------------------------------------------------
# The project
#-----------------------------------------------------------------
project (SolidFrame VERSION 12.3)

message("SolidFrame version: ${PROJECT_VERSION} - ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
#-----------------------------------------------------------------
# Set policies
#-----------------------------------------------------------------
cmake_policy(SET CMP0074 NEW)

#-----------------------------------------------------------------

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#-----------------------------------------------------------------
# Extra user definitions
#-----------------------------------------------------------------
set(EXTRA_COMPILE_OPTIONS "" CACHE STRING "Extra compiler options")
add_compile_options("SHELL: ${EXTRA_COMPILE_OPTIONS}")


set(EXTRA_LINK_OPTIONS "" CACHE STRING "Extra linker options")
add_link_options("SHELL: ${EXTRA_LINK_OPTIONS}")

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

#-----------------------------------------------------------------
# C++ Standard
#-----------------------------------------------------------------

if(NOT CMAKE_CXX_STANDARD)
    file (READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check/cpp20.cpp" source_code)

    set(CMAKE_CXX_STANDARD 20)
    include(CheckCXXSourceCompiles)
    CHECK_CXX_SOURCE_COMPILES("${source_code}" SOLID_COMPILE_CXX20)
    if(NOT SOLID_COMPILE_CXX20)
        set(CMAKE_CXX_STANDARD 17)
        set(CMAKE_CXX_STANDARD_REQUIRED ON)
        file (READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check/cpp17.cpp" source_code)
        CHECK_CXX_SOURCE_COMPILES("${source_code}" SOLID_COMPILE_CXX17)

        if(NOT SOLID_COMPILE_CXX17)
            message(FATAL_ERROR "SolidFrame needs at least CXX17!")
        endif()
    endif()
endif()

message("C++ Standard: ${CMAKE_CXX_STANDARD}")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#-----------------------------------------------------------------
# Prepare the external path
#-----------------------------------------------------------------

set(EXTERNAL_DIR "" CACHE PATH "External dependencies directory")
set(FORTIFY "ON" CACHE BOOL "Fortify C++")

message("Install directory: \"${CMAKE_INSTALL_PREFIX}\"")
message("External directory: \"${EXTERNAL_DIR}\"")
message("Fortify: \"${FORTIFY}\"")

list(APPEND CMAKE_PREFIX_PATH ${EXTERNAL_PATH})

#-----------------------------------------------------------------
# Prepare the definitions for build types
#-----------------------------------------------------------------

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to 'release' as none was specified.")
    set(CMAKE_BUILD_TYPE "release" CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    if(CMAKE_SYSTEM MATCHES "Windows*")
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
            "release" "debug" "maintain" "custom")
    else()
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "release" "debug" "maintain" "custom" "coverage")
    endif()
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE)

if(CMAKE_BUILD_TYPE STREQUAL "release")
    #force a single configuration type on Windows builds
    set(CMAKE_CONFIGURATION_TYPES "release")
    set(CONFIGURATION_TYPE "release")
    set(SOLID_HAS_STATISTICS TRUE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "maintain")
    #set(CMAKE_VERBOSE_MAKEFILE ON)
    set(SOLID_HAS_DEBUG TRUE)
    set(SOLID_HAS_STATISTICS TRUE)
    set(SOLID_HAS_ASSERT TRUE)

    #force a single configuration type on Windows builds
    set(CMAKE_CONFIGURATION_TYPES "RelWithDebInfo")
    set(CONFIGURATION_TYPE "RelWithDebInfo")
endif()


if(CMAKE_BUILD_TYPE STREQUAL "debug")
    #set(CMAKE_VERBOSE_MAKEFILE ON)
    set(SOLID_HAS_DEBUG TRUE)
    set(SOLID_HAS_NO_INLINES TRUE)
    set(SOLID_HAS_STATISTICS TRUE)
    set(SOLID_HAS_ASSERT TRUE)
    #force a single configuration type on Windows builds
    set(CMAKE_CONFIGURATION_TYPES "debug")
    set(CONFIGURATION_TYPE "debug")
endif()


if((NOT CMAKE_SYSTEM MATCHES "Windows*") AND CMAKE_BUILD_TYPE STREQUAL "coverage")
    #set(CMAKE_VERBOSE_MAKEFILE ON)
    set(SOLID_HAS_DEBUG TRUE)
    set(SOLID_HAS_NO_INLINES TRUE)
    set(SOLID_HAS_STATISTICS TRUE)
    set(SOLID_HAS_ASSERT TRUE)
    #force a single configuration type on Windows builds
    set(CMAKE_CONFIGURATION_TYPES "debug")
    set(CONFIGURATION_TYPE "debug")
    add_compile_options("SHELL: --coverage")
    add_link_options("SHELL: --coverage")
elseif(CMAKE_SYSTEM MATCHES "Windows*" AND CMAKE_BUILD_TYPE STREQUAL "coverage")
    message(FATAL_ERROR "\r\n === Coverage not yet supported for Windows systems ===\r\n")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "custom")
    #force a single configuration type on Windows builds
    set(CMAKE_CONFIGURATION_TYPES "custom")
    set(CONFIGURATION_TYPE "custom")
endif()

set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING
     "Add the native configuration"
     FORCE)
mark_as_advanced(CMAKE_CONFIGURATION_TYPES)

set(SOLID_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(SOLID_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(SOLID_VERSION_PATCH ${PROJECT_VERSION_PATCH})

set(SYSTEM_BASIC_LIBRARIES "")


option(SOLID_FRAME_AIO_REACTOR_USE_SPINLOCK "Use SpinLock on AIO Reactor" ON)
option(SOLID_MPRPC_USE_SHARED_PTR_MESSAGE "Use std::shared_ptr with mprpc::Message" OFF)

#-----------------------------------------------------------------
# Per OS configuration
#-----------------------------------------------------------------

message("")
message("CMake's system: ${CMAKE_SYSTEM}")
message("")

set(ON_FOUND FALSE)
set(SOLID_ON_FREEBSD FALSE)
set(SOLID_ON_DARWIN FALSE)
set(SOLID_ON_LINUX FALSE)
set(SOLID_ON_POSIX FALSE)
set(SOLID_ON_SOLARIS FALSE)
set(SOLID_ON_WINDOWS FALSE)

if(CMAKE_SYSTEM MATCHES "FreeBSD*")
    message("Detected system ON_FREEBSD")
    set(ON_FOUND TRUE)
    set(SOLID_ON_FREEBSD TRUE)
    set(SOLID_ON_POSIX TRUE)
    
    add_compile_options(
        "$<$<CONFIG:MAINTAIN>:SHELL: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -Wall -Wshorten-64-to-32 -Wunreachable-code -Wconditional-uninitialized>"
        "$<$<CONFIG:CUSTOM>:>"
    )

    set(SYSTEM_BASIC_LIBRARIES pmc rt Threads::Threads)
    
    find_library(EXECINFO_LIBRARY NAMES execinfo)
    
    if(EXECINFO_LIBRARY)
        message("Execinfo library found: ${EXECINFO_LIBRARY}")
        list(APPEND SYSTEM_BASIC_LIBRARIES ${EXECINFO_LIBRARY})
    endif()
endif()

if(CMAKE_SYSTEM MATCHES "Darwin*")
    message("Detected system ON_DARWIN")
    set(ON_FOUND TRUE)
    set(SOLID_ON_DARWIN TRUE)
    set(SOLID_ON_POSIX TRUE)
    
    add_compile_options(
        "$<$<CONFIG:MAINTAIN>:SHELL: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -Wall -Wshorten-64-to-32 -Wunreachable-code -Wconditional-uninitialized -Wabi>"
        "$<$<CONFIG:CUSTOM>:>"
    )
    
    string(APPEND CMAKE_EXE_LINKER_FLAGS "-framework ApplicationServices -framework CoreFoundation")
    
    set(SYSTEM_BASIC_LIBRARIES Threads::Threads)
endif()

if(CMAKE_SYSTEM MATCHES "Linux*")
    message("Detected system ON_LINUX")
    set(ON_FOUND TRUE)
    set(SOLID_ON_LINUX TRUE)
    set(SOLID_ON_POSIX TRUE)
    
    
    if(CMAKE_BUILD_TYPE STREQUAL "coverage")
        add_link_options("SHELL: -lgcov")
    endif()
    
    if(CMAKE_CXX_COMPILER MATCHES "clang")
        add_compile_options(
            "$<$<CONFIG:MAINTAIN>:SHELL: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -Wall -Wshorten-64-to-32 -Wunreachable-code -Wconditional-uninitialized -Wdelete-non-virtual-dtor -Wnon-virtual-dtor>"
            "$<$<CONFIG:CUSTOM>:>"
        )
    else()
        add_compile_options(
           "$<$<CONFIG:MAINTAIN>:SHELL: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -Wall -Wdelete-non-virtual-dtor -Wnon-virtual-dtor>"
           "$<$<CONFIG:CUSTOM>:>"
        )
    endif()
    
    set(SYSTEM_BASIC_LIBRARIES Threads::Threads rt)
    
    find_library(ATOMIC_LIBRARY NAMES atomic atomic.so.1 libatomic.so.1)
    
    if(ATOMIC_LIBRARY)
        message("Atomic library found: ${ATOMIC_LIBRARY}")
        list(APPEND SYSTEM_BASIC_LIBRARIES atomic)
    endif()
endif()

if(CMAKE_SYSTEM MATCHES "Windows*")
    message("Detected system ON_WINDOWS")
    set(ON_FOUND TRUE)
    set(SOLID_ON_WINDOWS TRUE)
    
    add_compile_options(
        #"$<$<CONFIG:RELASE>:/D_CRT_SECURE_NO_WARNINGS;/Ox>"
        #"$<$<CONFIG:DEBUG>:/D_CRT_SECURE_NO_WARNINGS;/Zi;/Od>"
        "$<$<CONFIG:MAINTAIN>:/D_CRT_SECURE_NO_WARNINGS;/Zi;/Od>"
        "$<$<CONFIG:CUSTOM>:>"
    )
    
    add_compile_options(-D_WIN32_WINNT=0x0601 -DWINVER=0x0601)
endif(CMAKE_SYSTEM MATCHES "Windows*")


if(CMAKE_SYSTEM MATCHES "Android*")
    message("Detected system ON_ANDROID")
    set(ON_FOUND TRUE)
    set(ON_CROSS TRUE)
    set(ON_ANDROID TRUE)
    set(SOLID_ON_LINUX TRUE)
    set(SOLID_ON_POSIX TRUE)
    set(SOLID_USE_GCC_BSWAP TRUE)
    set(NO_EXTERNAL TRUE)

    add_compile_options(
        "$<$<CONFIG:MAINTAIN>:SHELL: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -Wall>"
        "$<$<CONFIG:CUSTOM>:>"
    )

    set(SYSTEM_BASIC_LIBRARIES rt)
endif()

if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm") 
    add_compile_options("SHELL: -mcpu=native")
endif()

if(NOT ON_FOUND)
    message(FATAL_ERROR "\r\n === Unsupported system - please contact project owner ===\r\n")
endif(NOT ON_FOUND)


if(FORTIFY AND (SOLID_ON_LINUX OR SOLID_ON_FREEBSD OR SOLID_ON_DARWIN))
    message("Fortifying C++")
    include(CheckCXXSymbolExists)
    if(cxx_std_20 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    set(header version)
    else()
    set(header ciso646)
    endif()

    check_cxx_symbol_exists(_LIBCPP_VERSION ${header} LIBCPP)
    if(LIBCPP)
    # Logic for libc++
    endif()

    check_cxx_symbol_exists(__GLIBCXX__ ${header} GLIBCXX)
    if(GLIBCXX)
    # Logic for libstdc++

    add_compile_options(
        #-fpie -Wl,-pie
        #-fpic -shared
        "$<$<CONFIG:MAINTAIN>:SHELL: -D_FORTIFY_SOURCE=2 -D_GLIBCXX_ASSERTIONS -pipe -fasynchronous-unwind-tables -fexceptions -fstack-clash-protection -fcf-protection -Wl,-z,relro>"
        "$<$<CONFIG:DEBUG>:SHELL: -Wall -O2 -D_FORTIFY_SOURCE=2 -D_GLIBCXX_ASSERTIONS -pipe -fasynchronous-unwind-tables -fexceptions -fstack-clash-protection -fcf-protection -Wl,-z,relro>"
        "$<$<CONFIG:RELEASE>:SHELL: -Wall -D_FORTIFY_SOURCE=2 -D_GLIBCXX_ASSERTIONS -pipe -fasynchronous-unwind-tables -fexceptions -fstack-clash-protection -fcf-protection -Wl,-z,relro>"
        "$<$<CONFIG:CUSTOM>:>"
    )
    
    endif()
endif()

#-----------------------------------------------------------------
# Find external libraries
#-----------------------------------------------------------------

if(ON_ANDROID)
    set(EXTERNAL_DIR "${EXTERNAL_DIR}/${ANDROID_ABI}")
endif()

if(NOT NO_EXTERNAL)
    include(ExternalProject)
    
    include(cmake/build-snappy.cmake)
    include(cmake/build-cxxopts.cmake)

    include_directories(${CMAKE_BINARY_DIR}/external/include)

    if(EXTERNAL_DIR STREQUAL "" OR NOT EXISTS ${EXTERNAL_DIR})
        set(EXTERNAL_DIR "${CMAKE_CURRENT_BINARY_DIR}/external")

        include(cmake/build-openssl.cmake)
    else()
        #define dummy targets
        add_custom_target(build-openssl)

        include(cmake/find-openssl.cmake)
        
        include_directories(${EXTERNAL_DIR}/include)
    endif()

else()
    set(OPENSSL_FOUND TRUE)
endif()

#-----------------------------------------------------------------
# config.h support
#-----------------------------------------------------------------
if(NOT ON_CROSS)
    include(cmake/check.config.cmake)
elseif(ON_ANDROID)
    set(SOLID_USE_EPOLL TRUE)
    set(SOLID_USE_EVENTFD TRUE)
endif()

#-----------------------------------------------------------------
# Global include directories
#-----------------------------------------------------------------
include_directories(
    "${SolidFrame_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
)

#-----------------------------------------------------------------
# Global link directories
#-----------------------------------------------------------------
link_directories(
    ${EXTERNAL_DIR}/lib
)

#-----------------------------------------------------------------
# Static Analisys
#-----------------------------------------------------------------

include(cmake/clang-tidy.cmake)

#-----------------------------------------------------------------
# Testing support (CTest & CDash)
#-----------------------------------------------------------------
if(NOT ON_CROSS)
    find_program( MEMORYCHECK_COMMAND valgrind )
    set( MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full" )
    set( MEMORYCHECK_SUPPRESSIONS_FILE "${PROJECT_SOURCE_DIR}/cmake/valgrind_suppress.txt" )

    set(SOLID_TEST_ALL TRUE CACHE BOOL "Enable/Disable all tests")

    enable_testing()
    include(CTest)
else()
    set(SOLID_TEST_ALL FALSE)
endif()

#-----------------------------------------------------------------
# Project subdirectories
#-----------------------------------------------------------------

add_subdirectory (solid)

add_custom_target(libraries
    DEPENDS
    solid_system
    solid_utility
    solid_reflection_v1
    solid_serialization_v3
    solid_frame
    solid_frame_aio
    solid_frame_aio_openssl
    solid_frame_file
    solid_frame_mprpc
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/SolidFrameConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)

export(EXPORT SolidFrameConfig FILE SolidFrameConfig.cmake NAMESPACE SolidFrame::)

install(EXPORT SolidFrameConfig DESTINATION lib/cmake/SolidFrame COMPONENT Devel NAMESPACE SolidFrame::)



install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/SolidFrameConfigVersion.cmake"
    DESTINATION
        lib/cmake/SolidFrame
    COMPONENT
        Devel
)

if(NOT ON_CROSS)
    add_subdirectory (examples)
    add_subdirectory (tutorials)
endif()

include(cmake/clang-format.cmake)

