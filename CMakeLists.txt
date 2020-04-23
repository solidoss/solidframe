cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
#-----------------------------------------------------------------
# The project
#-----------------------------------------------------------------
project (SolidFrame VERSION 6.1)

execute_process(
    COMMAND "git" "rev-parse" "HEAD"
    WORKING_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION ${VERSION}
    OUTPUT_STRIP_TRAILING_WHITESPACE)

set(PROJECT_VERSION_PATCH "${GIT_VERSION}")

message("SolidFrame version: ${PROJECT_VERSION} - ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
#-----------------------------------------------------------------
# Set policies
#-----------------------------------------------------------------
if(NOT ${CMAKE_VERSION} VERSION_LESS "3.12.0") 
    cmake_policy(SET CMP0074 NEW)
endif()

#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Extra user definitions
#-----------------------------------------------------------------
set(EXTRA_COMPILE_OPTIONS "" CACHE STRING "Extra compiler options")
add_compile_options(${EXTRA_COMPILE_OPTIONS})


set(EXTRA_LINK_OPTIONS "" CACHE STRING "Extra linker options")
if (CMAKE_VERSION VERSION_LESS "3.13")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${EXTRA_LINK_OPTIONS}")
else()
    add_link_options(${EXTRA_LINK_OPTIONS})
endif()

#-----------------------------------------------------------------
# Prepare the external path
#-----------------------------------------------------------------

set(EXTERNAL_DIR "${EXTERNAL_DIR}" CACHE PATH "External dependencies directory")

message("Install path: \"${CMAKE_INSTALL_PREFIX}\"")
message("External path: \"${EXTERNAL_DIR}\"")

#-----------------------------------------------------------------
# Prepare the definitions for build types
#-----------------------------------------------------------------

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "release")
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE)

set(CMAKE_CONFIGURATION_TYPES "release" CACHE STRING "" FORCE)
set(CONFIGURATION_TYPE "release")

if(CMAKE_BUILD_TYPE STREQUAL "maintain")
    set(CMAKE_VERBOSE_MAKEFILE ON)
    set(SOLID_HAS_DEBUG TRUE)
    set(SOLID_HAS_STATISTICS TRUE)
    set(SOLID_HAS_ASSERT TRUE)

    #force a single configuration type on Windows builds
    set(CMAKE_CONFIGURATION_TYPES "release" CACHE STRING "" FORCE)
    set(CONFIGURATION_TYPE "release")
endif(CMAKE_BUILD_TYPE STREQUAL "maintain")


if(CMAKE_BUILD_TYPE STREQUAL "debug")
    set(CMAKE_VERBOSE_MAKEFILE ON)
    set(SOLID_HAS_DEBUG TRUE)
    set(SOLID_HAS_NO_INLINES TRUE)
    set(SOLID_HAS_STATISTICS TRUE)
    set(SOLID_HAS_ASSERT TRUE)
    #force a single configuration type on Windows builds
    set(CMAKE_CONFIGURATION_TYPES "debug" CACHE STRING "" FORCE)
    set(CONFIGURATION_TYPE "debug")
endif(CMAKE_BUILD_TYPE STREQUAL "debug")

set(SOLID_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(SOLID_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(SOLID_VERSION_PATCH ${PROJECT_VERSION_PATCH})

set(SYSTEM_BASIC_LIBRARIES "")
set(SYSTEM_DYNAMIC_LOAD_LIBRARY "")

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
        "$<$<CONFIG:RELASE>:-O3>"
        "$<$<CONFIG:DEBUG>:-Wall;-Wshorten-64-to-32;-Wunreachable-code;-Wconditional-uninitialized;-Wreorder;-Wreturn-type;-Wunused-variable;-Winline;-g3>"
        "$<$<CONFIG:MAINTAIN>:-Wall;-Wshorten-64-to-32;-Wunreachable-code;-Wconditional-uninitialized;-O3;-g3>"
    )

    set(SYSTEM_BASIC_LIBRARIES pmc pthread rt)
    
    find_library(EXECINFO_LIBRARY NAMES execinfo)
    
    if(EXECINFO_LIBRARY)
        message("Execinfo library found: ${EXECINFO_LIBRARY}")
        list(APPEND SYSTEM_BASIC_LIBRARIES ${EXECINFO_LIBRARY})
    endif()
endif(CMAKE_SYSTEM MATCHES "FreeBSD*")

if(CMAKE_SYSTEM MATCHES "Darwin*")
    message("Detected system ON_DARWIN")
    set(ON_FOUND TRUE)
    set(SOLID_ON_DARWIN TRUE)
    set(SOLID_ON_POSIX TRUE)
    
    add_compile_options(
        "$<$<CONFIG:RELASE>:-O3>"
        "$<$<CONFIG:DEBUG>:-Wall;-Wshorten-64-to-32;-Wunreachable-code;-Wconditional-uninitialized;-Wreorder;-Wreturn-type;-Wunused-variable;-Winline;-g3>"
        "$<$<CONFIG:MAINTAIN>:-Wall;-Wshorten-64-to-32;-Wunreachable-code;-Wconditional-uninitialized;-Wabi;-O3;-g3>"
    )
    
    string(APPEND CMAKE_EXE_LINKER_FLAGS "-framework ApplicationServices -framework CoreFoundation")
    
    set(SYSTEM_BASIC_LIBRARIES pthread)
endif(CMAKE_SYSTEM MATCHES "Darwin*")

if(CMAKE_SYSTEM MATCHES "Linux*")
    message("Detected system ON_LINUX")
    set(ON_FOUND TRUE)
    set(SOLID_ON_LINUX TRUE)
    set(SOLID_ON_POSIX TRUE)
    
    

    if(CMAKE_CXX_COMPILER MATCHES "clang")
        add_compile_options(
            "$<$<CONFIG:RELASE>:-O3>"
            "$<$<CONFIG:DEBUG>:-Wall;-Wshorten-64-to-32;-Wunreachable-code;-Wconditional-uninitialized;-Wreorder;-Wreturn-type;-Wunused-variable;-Winline;-Wdelete-non-virtual-dtor;-Wnon-virtual-dtor;-g3>"
            "$<$<CONFIG:MAINTAIN>:-Wall;-Wshorten-64-to-32;-Wunreachable-code;-Wconditional-uninitialized;-Wdelete-non-virtual-dtor;-Wnon-virtual-dtor;-O3;-g3>"
        )
    else()
        add_compile_options(
            "$<$<CONFIG:RELASE>:-O3>"
            "$<$<CONFIG:DEBUG>:-Wall;-Wreorder;-Wreturn-type;-Wunused-variable;-Winline;-Wdelete-non-virtual-dtor;-Wnon-virtual-dtor;-g3>"
            "$<$<CONFIG:MAINTAIN>:-Wall;-Wdelete-non-virtual-dtor;-Wnon-virtual-dtor;-O3;-g3>"
        )
    endif()
    
    set(SYSTEM_BASIC_LIBRARIES pthread rt)
    
    find_library(ATOMIC_LIBRARY NAMES atomic atomic.so.1 libatomic.so.1)
    
    if(ATOMIC_LIBRARY)
        message("Atomic library found: ${ATOMIC_LIBRARY}")
        list(APPEND SYSTEM_BASIC_LIBRARIES atomic)
    endif()
    
    set(SYSTEM_DYNAMIC_LOAD_LIBRARY dl)
endif(CMAKE_SYSTEM MATCHES "Linux*")

if(CMAKE_SYSTEM MATCHES "Windows*")
    message("Detected system ON_WINDOWS")
    set(ON_FOUND TRUE)
    set(SOLID_ON_WINDOWS TRUE)
    
    add_compile_options(
        "$<$<CONFIG:RELASE>:/D_CRT_SECURE_NO_WARNINGS;/Ox>"
        "$<$<CONFIG:DEBUG>:/D_CRT_SECURE_NO_WARNINGS;/Zi;/Od>"
        "$<$<CONFIG:MAINTAIN>:/D_CRT_SECURE_NO_WARNINGS;/Zi;/Od>"
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
        "$<$<CONFIG:RELASE>:-O3>"
        "$<$<CONFIG:DEBUG>:-Wall;-Wreorder;-Wreturn-type;-Wunused-variable;-Winline;-g3>"
        "$<$<CONFIG:MAINTAIN>:-Wall;-O3;-g3>"
    )

    set(SYSTEM_BASIC_LIBRARIES pthread rt)
    set(SYSTEM_DYNAMIC_LOAD_LIBRARY dl)
endif(CMAKE_SYSTEM MATCHES "Android*")

if(NOT ON_FOUND)
    message("\r\n === Unsupported system - please contact project owner ===\r\n")
    return()
endif(NOT ON_FOUND)

#-----------------------------------------------------------------
# Find external libraries
#-----------------------------------------------------------------

if(NOT NO_EXTERNAL)
    include(ExternalProject)

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    include(cmake/build_snappy.cmake)
    include(cmake/build_cxxopts.cmake)

    include_directories(${CMAKE_BINARY_DIR}/external/include)

    if(EXTERNAL_DIR STREQUAL "")
        set(EXTERNAL_DIR "${CMAKE_CURRENT_BINARY_DIR}/external")

        include(cmake/build_openssl.cmake)
    else()
        #define dummy targets
        add_custom_target(build-openssl)

        include(cmake/find_openssl.cmake)
    endif()

else()
    set(OPENSSL_FOUND TRUE)
endif()

#-----------------------------------------------------------------
# install prefix
#-----------------------------------------------------------------

if(ON_ANDROID)
    set(EXTERNAL_DIR "${EXTERNAL_DIR}/${ANDROID_ABI}")
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

if(ON_CROSS)
    set(SOLID_TEST_NONE TRUE)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/solid/solid_config.hpp)


install (FILES ${CMAKE_CURRENT_BINARY_DIR}/solid/solid_config.hpp DESTINATION include/solid)

#-----------------------------------------------------------------
# C++ Standard
#-----------------------------------------------------------------

if (CMAKE_VERSION VERSION_LESS "3.1")
    set (CMAKE_CXX_FLAGS "--std=c++14 ${CMAKE_CXX_FLAGS}")
else ()
   set (CMAKE_CXX_STANDARD 14)
endif ()

#-----------------------------------------------------------------

#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Global include directories
#-----------------------------------------------------------------
include_directories(
    "${SolidFrame_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${EXTERNAL_DIR}/include"
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Global link directories
#-----------------------------------------------------------------
link_directories(
    "${EXTERNAL_DIR}/lib"
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
    enable_testing()
    include(CTest)
endif()

#-----------------------------------------------------------------
# Project subdirectories
#-----------------------------------------------------------------

add_subdirectory (solid)

add_custom_target(libraries
    DEPENDS
    solid_system
    solid_utility
    solid_serialization_v1
    solid_serialization_v2
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

export(EXPORT SolidFrameConfig FILE SolidFrameConfig.cmake)

install(EXPORT SolidFrameConfig DESTINATION lib/cmake/SolidFrame)

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

