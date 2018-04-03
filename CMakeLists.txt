cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
#-----------------------------------------------------------------
# The project
#-----------------------------------------------------------------
project (SolidFrame VERSION 3.0)

execute_process(
    COMMAND "git" "rev-parse" "HEAD"
    WORKING_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION ${VERSION}
    OUTPUT_STRIP_TRAILING_WHITESPACE)

set(PROJECT_VERSION_PATCH "${GIT_VERSION}")

message("SolidFrame version: ${PROJECT_VERSION} - ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

#-----------------------------------------------------------------
# Extra user definitions
#-----------------------------------------------------------------
set(EXTRA_DEFINITIONS "${UDEFS}" CACHE STRING "Extra compiler definitions")
add_definitions(${EXTRA_DEFINITIONS})
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Prepare the external path
#-----------------------------------------------------------------

if(NOT UEXTERNAL OR UEXTERNAL STREQUAL "")
    message("Extern path not given")
    set(EXTERNAL_PATH "" CACHE PATH "External prerequisites path")
else()
    set(EXTERNAL_PATH "${UEXTERNAL}/" CACHE PATH "External prerequisites path")
    message("External path: \"${EXTERNAL_PATH}\"")
endif()

message("Install path: \"${CMAKE_INSTALL_PREFIX}\"")

#-----------------------------------------------------------------
# Prepare the definitions for build types
#-----------------------------------------------------------------

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "release")
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE)

#force a single configuration type on Windows builds
set(CMAKE_CONFIGURATION_TYPES "${CMAKE_BUILD_TYPE}" CACHE STRING "" FORCE)

if(CMAKE_BUILD_TYPE STREQUAL "maintain")
    set(CMAKE_VERBOSE_MAKEFILE ON)
    set(SOLID_HAS_DEBUG TRUE)
    set(SOLID_HAS_STATISTICS TRUE)
    set(SOLID_HAS_ASSERT TRUE)

    #force a single configuration type on Windows builds
    set(CMAKE_CONFIGURATION_TYPES "release" CACHE STRING "" FORCE)
endif(CMAKE_BUILD_TYPE STREQUAL "maintain")


if(CMAKE_BUILD_TYPE STREQUAL "debug")
    set(CMAKE_VERBOSE_MAKEFILE ON)
    set(SOLID_HAS_DEBUG TRUE)
    set(SOLID_HAS_NO_INLINES TRUE)
    set(SOLID_HAS_STATISTICS TRUE)
    set(SOLID_HAS_ASSERT TRUE)
    #force a single configuration type on Windows builds
    set(CMAKE_CONFIGURATION_TYPES "debug" CACHE STRING "" FORCE)
endif(CMAKE_BUILD_TYPE STREQUAL "debug")


if(CMAKE_BUILD_TYPE STREQUAL "nolog")
    set(CMAKE_VERBOSE_MAKEFILE ON)
    set(SOLID_HAS_NO_INLINES TRUE)
    set(SOLID_HAS_ASSERT TRUE)
    #force a single configuration type on Windows builds
    set(CMAKE_CONFIGURATION_TYPES "debug" CACHE STRING "" FORCE)
endif(CMAKE_BUILD_TYPE STREQUAL "nolog")

set(SOLID_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(SOLID_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(SOLID_VERSION_PATCH ${PROJECT_VERSION_PATCH})

set(SYS_DEBUG_FLAGS "")
set(SYS_MAINTAIN_FLAGS "")
set(SYS_OPTIMIZED_FLAGS "")
set(SYS_NOLOG_FLAGS "")
set(SYS_RELEASE_FLAGS "")

set(SYS_DEBUG_LINKER_FLAGS "")
set(SYS_MAINTAIN_LINKER_FLAGS "")
set(SYS_OPTIMIZED_LINKER_FLAGS "")
set(SYS_NOLOG_LINKER_FLAGS "")
set(SYS_RELEASE_LINKER_FLAGS "")

set(SYS_BASIC_LIBS "")
set(SYS_DYNAMIC_LOAD_LIB "")
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

if(CMAKE_SYSTEM MATCHES "SunOS*")
    message("Detected system ON_SOLARIS")
    set(ON_FOUND TRUE)
    set(SOLID_ON_SOLARIS TRUE)
    set(SOLID_ON_POSIX TRUE)
    set(SYS_DEBUG_FLAGS "-features=extensions -library=stlport4 -mt -g")
    set(SYS_MAINTAIN_FLAGS "-features=extensions -library=stlport4 -mt")
    set(SYS_OPTIMIZED_FLAGS "-features=extensions -library=stlport4 -mt")
    set(SYS_NOLOG_FLAGS "-features=extensions -library=stlport4 -mt")
    set(SYS_RELEASE_FLAGS "-features=extensions -library=stlport4 -mt")

    set(SYS_BASIC_LIBS pthread socket nsl)

endif(CMAKE_SYSTEM MATCHES "SunOS*")

if(CMAKE_SYSTEM MATCHES "FreeBSD*")
    message("Detected system ON_FREEBSD")
    set(ON_FOUND TRUE)
    set(SOLID_ON_FREEBSD TRUE)
    set(SOLID_ON_POSIX TRUE)
    set(SYS_DEBUG_FLAGS "-Wreorder -Wreturn-type -Wunused-variable -Winline -g3")
    set(SYS_MAINTAIN_FLAGS "-Wall -Wextra -Wabi -O3 -g3")
    set(SYS_OPTIMIZED_FLAGS "-O3 -g3")
    set(SYS_NOLOG_FLAGS "-g3")
    set(SYS_RELEASE_FLAGS "-O3")

    set(SYS_DEBUG_LINKER_FLAGS "")
    set(SYS_MAINTAIN_LINKER_FLAGS "-Wl,--warn-unresolved-symbols,--warn-once")
    set(SYS_OPTIMIZED_LINKER_FLAGS "")
    set(SYS_NOLOG_LINKER_FLAGS "")
    set(SYS_RELEASE_LINKER_FLAGS "")

    set(SYS_BASIC_LIBS pmc pthread rt)
endif(CMAKE_SYSTEM MATCHES "FreeBSD*")

if(CMAKE_SYSTEM MATCHES "Darwin*")
    message("Detected system ON_DARWIN")
    set(ON_FOUND TRUE)
    set(SOLID_ON_DARWIN TRUE)
    set(SOLID_ON_POSIX TRUE)
    set(SYS_DEBUG_FLAGS "-Wreorder -Wreturn-type -Wunused-variable -Winline -g3")
    set(SYS_MAINTAIN_FLAGS "-Wall -Wabi -O3 -g3")
    set(SYS_OPTIMIZED_FLAGS "-O3 -g3")
    set(SYS_NOLOG_FLAGS "-g3")
    set(SYS_RELEASE_FLAGS "-O3")

    set(SYS_DEBUG_LINKER_FLAGS "-framework ApplicationServices -framework CoreFoundation")
    set(SYS_MAINTAIN_LINKER_FLAGS "-framework ApplicationServices -framework CoreFoundation")
    set(SYS_OPTIMIZED_LINKER_FLAGS "-framework ApplicationServices -framework CoreFoundation")
    set(SYS_NOLOG_LINKER_FLAGS "-framework ApplicationServices -framework CoreFoundation")
    set(SYS_RELEASE_LINKER_FLAGS "-framework ApplicationServices -framework CoreFoundation")

    set(SYS_BASIC_LIBS pthread)
endif(CMAKE_SYSTEM MATCHES "Darwin*")

if(CMAKE_SYSTEM MATCHES "Linux*")
    message("Detected system ON_LINUX")
    set(ON_FOUND TRUE)
    set(SOLID_ON_LINUX TRUE)
    set(SOLID_ON_POSIX TRUE)


    set(SYS_DEBUG_FLAGS "-Wreorder -Wreturn-type -Wunused-variable -Winline -g3")
    if(CMAKE_CXX_COMPILER MATCHES "clang")
        set(SYS_DEBUG_FLAGS "-Wall -Wshorten-64-to-32 -Wunreachable-code -Wconditional-uninitialized -Wreorder -Wreturn-type -Wunused-variable -Winline -g3")
        set(SYS_MAINTAIN_FLAGS "-Wall -Wshorten-64-to-32 -Wunreachable-code -Wconditional-uninitialized -Wabi -O3 -g3")
    else()
        set(SYS_DEBUG_FLAGS "-Wall -Wreorder -Wreturn-type -Wunused-variable -Winline -g3")
        set(SYS_MAINTAIN_FLAGS "-Wall -Wabi -O3 -g3")
    endif()
    set(SYS_OPTIMIZED_FLAGS "-O3 -g3")
    set(SYS_NOLOG_FLAGS "-g3")
    set(SYS_RELEASE_FLAGS "-O3")

    set(SYS_DEBUG_LINKER_FLAGS "")# -D_GLIBCXX_DEBUG
    set(SYS_MAINTAIN_LINKER_FLAGS "-Wl,--warn-unresolved-symbols,--warn-once")
    set(SYS_OPTIMIZED_LINKER_FLAGS "")
    set(SYS_NOLOG_LINKER_FLAGS "")
    set(SYS_RELEASE_LINKER_FLAGS "-O3")

    set(SYS_BASIC_LIBS pthread rt)
    set(SYS_DYNAMIC_LOAD_LIB dl)
endif(CMAKE_SYSTEM MATCHES "Linux*")

if(CMAKE_SYSTEM MATCHES "Windows*")
    message("Detected system ON_WINDOWS")
    set(ON_FOUND TRUE)
    set(SOLID_ON_WINDOWS TRUE)
    set(SYS_DEBUG_FLAGS "-D_CRT_SECURE_NO_WARNINGS /Zi /Od")
    set(SYS_MAINTAIN_FLAGS "-D_CRT_SECURE_NO_WARNINGS /Zi /Od")
    set(SYS_OPTIMIZED_FLAGS "-D_CRT_SECURE_NO_WARNINGS /Ox  /Zi")
    set(SYS_NOLOG_FLAGS "-D_CRT_SECURE_NO_WARNINGS /Zi /Od")
    set(SYS_RELEASE_FLAGS "-D_CRT_SECURE_NO_WARNINGS /Ox")

    set(SYS_DEBUG_LINKER_FLAGS "")
    set(SYS_MAINTAIN_LINKER_FLAGS "")
    set(SYS_OPTIMIZED_LINKER_FLAGS "")
    set(SYS_NOLOG_LINKER_FLAGS "")
    set(SYS_RELEASE_LINKER_FLAGS "")

    #add_definitions(-D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00)
endif(CMAKE_SYSTEM MATCHES "Windows*")


if(CMAKE_SYSTEM MATCHES "Android*")
    message("Detected system ON_ANDROID")
    set(ON_FOUND TRUE)
    set(ON_CROSS TRUE)
    set(ON_ANDROID TRUE)
    set(SOLID_ON_LINUX TRUE)
    set(SOLID_ON_POSIX TRUE)
    set(NO_EXTERNAL TRUE)


    set(SYS_DEBUG_FLAGS "-Wreorder -Wreturn-type -Wunused-variable -Winline -g3")# -D_GLIBCXX_DEBUG
    set(SYS_MAINTAIN_FLAGS "-Wall -Wabi -O3 -g3")
    set(SYS_OPTIMIZED_FLAGS "-O3 -g3")
    set(SYS_NOLOG_FLAGS "-g3")
    set(SYS_RELEASE_FLAGS "-O3")

    set(SYS_DEBUG_LINKER_FLAGS "")# -D_GLIBCXX_DEBUG
    set(SYS_MAINTAIN_LINKER_FLAGS "-Wl,--warn-unresolved-symbols,--warn-once")
    set(SYS_OPTIMIZED_LINKER_FLAGS "")
    set(SYS_NOLOG_LINKER_FLAGS "")
    set(SYS_RELEASE_LINKER_FLAGS "-O3")

    set(SYS_BASIC_LIBS pthread rt)
    set(SYS_DYNAMIC_LOAD_LIB dl)
endif(CMAKE_SYSTEM MATCHES "Android*")

if(NOT ON_FOUND)
    message("\r\n === Sorry unsupported system - please contact project owner ===\r\n")
    return()
endif(NOT ON_FOUND)

#-----------------------------------------------------------------
# Find external libraries
#-----------------------------------------------------------------

if(NOT NO_EXTERNAL)
    include(ExternalProject)

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    include(cmake/build_snappy.cmake)

    include_directories(${CMAKE_BINARY_DIR}/external/include)

    if(EXTERNAL_PATH STREQUAL "")
        set(EXTERNAL_PATH "${CMAKE_CURRENT_BINARY_DIR}/external")

        include(cmake/build_boost.cmake)
        include(cmake/build_openssl.cmake)
    else()
        #define dummy targets
        add_custom_target(build_boost)
        add_custom_target(build_openssl)
        #add_custom_target(build_snappy)

        include(cmake/find_boost.cmake)
        include(cmake/find_openssl.cmake)
        #include(cmake/find_snappy.cmake)

        message("")
        if(${Boost_FOUND})
            message("Boost found with version: ${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION}")
        else()
            message("Boost not found - some targets will not compile")
        endif()

        if(OPENSSL_FOUND)
            message("OpenSSL found with version: ${OPENSSL_VERSION}")
        else()
            message("OpenSSL not found - some targets will not compile")
        endif()

        message("")
    endif()

else()
    set(OPENSSL_FOUND TRUE)
endif()

#-----------------------------------------------------------------
# install prefix
#-----------------------------------------------------------------

if(ON_ANDROID)
    set(EXTERNAL_PATH "${EXTERNAL_PATH}/${ANDROID_ABI}")
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

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/solid/solid_config.hpp)


install (FILES ${CMAKE_CURRENT_BINARY_DIR}/solid/solid_config.hpp DESTINATION include/solid)

if (CMAKE_VERSION VERSION_LESS "3.1")
    set (CMAKE_CXX_FLAGS "--std=c++14 ${CMAKE_CXX_FLAGS}")
else ()
   set (CMAKE_CXX_STANDARD 14)
endif ()

#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Debug build
#-----------------------------------------------------------------
set(CMAKE_CXX_FLAGS_DEBUG
    "${SYS_DEBUG_FLAGS}"
    CACHE STRING "Flags used by the C++ compiler during debug builds."
    FORCE
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Maintain build
#-----------------------------------------------------------------
set(CMAKE_CXX_FLAGS_MAINTAIN
    "${SYS_MAINTAIN_FLAGS}"
    CACHE STRING "Flags used by the C++ compiler during maintain builds."
    FORCE
)
set(CMAKE_EXE_LINKER_FLAGS_MAINTAIN
    "${SYS_MAINTAIN_LINKER_FLAGS}"
    CACHE STRING "Flags used for linking binaries during maintain builds."
    FORCE
)
set(CMAKE_SHARED_LINKER_FLAGS_MAINTAIN
    "${SYS_MAINTAIN_LINKER_FLAGS}"
    CACHE STRING "Flags used by the shared libraries linker during maintain builds."
    FORCE
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Optimized build
#-----------------------------------------------------------------
set(CMAKE_CXX_FLAGS_OPTIMIZED
    "${SYS_OPTIMIZED_FLAGS}"
    CACHE STRING "Flags used by the C++ compiler during optimized builds."
    FORCE
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Release build
#-----------------------------------------------------------------
set(CMAKE_CXX_FLAGS_RELEASE
    "${SYS_RELEASE_FLAGS}"
    CACHE STRING "Flags used by the C++ compiler during release builds."
    FORCE
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Nolog build
#-----------------------------------------------------------------
set(CMAKE_CXX_FLAGS_NOLOG
    "${SYS_NOLOG_FLAGS}"
    CACHE STRING "Flags used by the C++ compiler during nolog builds."
    FORCE
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
mark_as_advanced(
    CMAKE_CXX_FLAGS_NOLOG
    CMAKE_C_FLAGS_NOLOG
    CMAKE_EXE_LINKER_FLAGS_NOLOG
    CMAKE_SHARED_LINKER_FLAGS_NOLOG
)
#-----------------------------------------------------------------
mark_as_advanced(
    CMAKE_CXX_FLAGS_MAINTAIN
    CMAKE_C_FLAGS_MAINTAIN
    CMAKE_EXE_LINKER_FLAGS_MAINTAIN
    CMAKE_SHARED_LINKER_FLAGS_MAINTAIN
)
#-----------------------------------------------------------------
mark_as_advanced(
    CMAKE_CXX_FLAGS_OPTIMIZED
    CMAKE_C_FLAGS_OPTIMIZED
    CMAKE_EXE_LINKER_FLAGS_OPTIMIZED
    CMAKE_SHARED_LINKER_FLAGS_OPTIMIZED
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Update the documentation string of CMAKE_BUILD_TYPE for GUIs
#-----------------------------------------------------------------
set(CMAKE_BUILD_TYPE
    "${CMAKE_BUILD_TYPE}"
    CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel nolog maintain optimized:"
    FORCE
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Global include directories
#-----------------------------------------------------------------
include_directories(
    ${SolidFrame_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${EXTERNAL_PATH}/include/
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Global link directories
#-----------------------------------------------------------------
link_directories(
    ${EXTERNAL_PATH}/lib
)

#-----------------------------------------------------------------
# Testing support (CTest & CDash)
#-----------------------------------------------------------------
if(NOT ON_CROSS)
    find_program( MEMORYCHECK_COMMAND valgrind )
    set( MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full" )
    set( MEMORYCHECK_SUPPRESSIONS_FILE "${PROJECT_SOURCE_DIR}/cmake/valgrind_suppress.txt" )

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
    solid_frame_mpipc
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

