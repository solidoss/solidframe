cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
#-----------------------------------------------------------------
# The project
#-----------------------------------------------------------------
project (SolidFrame VERSION 2.1)

exec_program(
    "git"
    ${CMAKE_CURRENT_SOURCE_DIR}
    ARGS "rev-parse HEAD"
    OUTPUT_VARIABLE GIT_VERSION ${VERSION} )

set(PROJECT_VERSION_PATCH "${GIT_VERSION}")

message("SolidFrame version: ${PROJECT_VERSION} - ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

#-----------------------------------------------------------------
# Extra user definitions
#-----------------------------------------------------------------
set(EXTRA_DEFINITIONS "${UDEFS}" CACHE STRING "Extra compiler definitions")
add_definitions(${EXTRA_DEFINITIONS})
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Prepare the extern path
#-----------------------------------------------------------------

if(NOT UEXTERN OR UEXTERN STREQUAL "")
    message("Extern path not given")
    set(EXTERN_PATH "" CACHE PATH "External prerequisites path")
else()
    set(EXTERN_PATH "${UEXTERN}/" CACHE PATH "External prerequisites path")
    message("Extern path: \"${EXTERN_PATH}\"")
endif()


if(NOT UINSTALL_PREFIX OR UINSTALL_PREFIX STREQUAL "")
    set(UINSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
    message("Default install path: \"${UINSTALL_PREFIX}\"")
endif()


set(INSTALL_PATH "${UINSTALL_PREFIX}/" CACHE PATH "External prerequisites path")

message("Install path: \"${INSTALL_PATH}\"")

#-----------------------------------------------------------------
# Prepare the definitions for build types
#-----------------------------------------------------------------

string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE)

if(CMAKE_BUILD_TYPE STREQUAL "maintain")
    set(CMAKE_VERBOSE_MAKEFILE ON)
    set(SOLID_HAS_DEBUG TRUE)
    set(SOLID_HAS_STATISTICS TRUE)
    set(SOLID_HAS_ASSERT TRUE)
endif(CMAKE_BUILD_TYPE STREQUAL "maintain")


if(CMAKE_BUILD_TYPE STREQUAL "debug")
    set(CMAKE_VERBOSE_MAKEFILE ON)
    set(SOLID_HAS_DEBUG TRUE)
    set(SOLID_HAS_NO_INLINES TRUE)
    set(SOLID_HAS_STATISTICS TRUE)
    set(SOLID_HAS_ASSERT TRUE)
endif(CMAKE_BUILD_TYPE STREQUAL "debug")


if(CMAKE_BUILD_TYPE STREQUAL "nolog")
    set(CMAKE_VERBOSE_MAKEFILE ON)
    set(SOLID_HAS_NO_INLINES TRUE)
    set(SOLID_HAS_ASSERT TRUE)
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
    set(SYS_MAINTAIN_FLAGS "-Wall -Wabi -O3 -g3")
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
endif(CMAKE_SYSTEM MATCHES "Linux*")

if(CMAKE_SYSTEM MATCHES "Windows*")
    message("Detected system ON_WINDOWS")
    set(ON_FOUND TRUE)
    set(SOLID_ON_WINDOWS TRUE)
    set(SYS_DEBUG_FLAGS "-DSOLID_ON_WINDOWS -D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0600 /Zi /Od")
    set(SYS_MAINTAIN_FLAGS "-DSOLID_ON_WINDOWS -D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0600  /Zi /Od")
    set(SYS_OPTIMIZED_FLAGS "-DSOLID_ON_WINDOWS -D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0600 /Ox  /Zi")
    set(SYS_NOLOG_FLAGS "-DSOLID_ON_WINDOWS -D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0600 /Zi /Od")
    set(SYS_RELEASE_FLAGS "-DSOLID_ON_WINDOWS -D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0600 /Ox")

    set(SYS_DEBUG_LINKER_FLAGS "")
    set(SYS_MAINTAIN_LINKER_FLAGS "")
    set(SYS_OPTIMIZED_LINKER_FLAGS "")
    set(SYS_NOLOG_LINKER_FLAGS "")
    set(SYS_RELEASE_LINKER_FLAGS "")

    set(SYS_BASIC_LIBS Ws2_32 ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY})
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

message("")

if(NOT ON_FOUND)
    message("\r\n === Sorry unsupported system - please contact project owner ===\r\n")
    return()
else(NOT ON_FOUND)

endif(NOT ON_FOUND)

#-----------------------------------------------------------------
# Find external libraries
#-----------------------------------------------------------------

if(NOT NO_EXTERNAL)
    
    if(EXTERN_PATH STREQUAL "")
        set(EXTERN_PATH "${CMAKE_CURRENT_BINARY_DIR}/external")
        include(ExternalProject)

        set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
        include(cmake/build_boost.cmake)
        include(cmake/build_openssl.cmake)
        include(cmake/build_snappy.cmake)
    else()
        #define dummy targets
        add_custom_target(build_boost)
        add_custom_target(build_openssl)
        add_custom_target(build_snappy)
    
        include(cmake/find_boost.cmake)
        include(cmake/find_openssl.cmake)
        include(cmake/find_snappy.cmake)

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
if(NOT (INSTALL_PATH STREQUAL ""))
    set(CMAKE_INSTALL_PREFIX "${INSTALL_PATH}")

    if(ON_ANDROID)
        set(CMAKE_INSTALL_PREFIX "${INSTALL_PATH}/${ANDROID_ABI}")
    endif()

    message("Using install prefix: ${CMAKE_INSTALL_PREFIX}")
endif(NOT (INSTALL_PATH STREQUAL ""))

if(ON_ANDROID)
    set(EXTERN_PATH "${EXTERN_PATH}/${ANDROID_ABI}")
endif()

#-----------------------------------------------------------------
# config.h support
#-----------------------------------------------------------------
if(NOT ON_CROSS)
    include(cmake/check.config.cmake)
elseif(ON_ANDROID)
    set(SOLID_USE_EPOLL TRUE)
    set(SOLID_USE_CPP11 TRUE)
    set(SOLID_USE_EVENTFD TRUE)
    set(SOLID_USE_CPP11FLAG TRUE)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/solid/solid_config.hpp)


install (FILES ${CMAKE_CURRENT_BINARY_DIR}/solid/solid_config.hpp DESTINATION include/solid)
install (EXPORT SolidFrameConfig DESTINATION lib/cmake/SolidFrame)

if (CMAKE_VERSION VERSION_LESS "3.1")
    if(SOLID_USE_CPP11FLAG)
        set (CMAKE_CXX_FLAGS "--std=c++11 ${CMAKE_CXX_FLAGS}")
    else()
        message(FATAL_ERROR "SolidFrame framework needs C++11 enabled compiler")
    endif()
else ()
   set (CMAKE_CXX_STANDARD 11)
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
    ${EXTERN_PATH}/include/
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Global link directories
#-----------------------------------------------------------------
link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${EXTERN_PATH}/lib
)

#-----------------------------------------------------------------
# Testing support (CTest & CDash)
#-----------------------------------------------------------------
if(NOT ON_CROSS)
    ENABLE_TESTING()
    INCLUDE(CTest)
endif()

#-----------------------------------------------------------------
# Project subdirectories
#-----------------------------------------------------------------

add_subdirectory (solid)

export(EXPORT SolidFrameConfig FILE SolidFrameConfig.cmake)

if(NOT ON_CROSS)
    add_subdirectory (examples)
    add_subdirectory (tutorials)
endif()


