cmake_minimum_required(VERSION 2.6)
#-----------------------------------------------------------------
# The project
#-----------------------------------------------------------------
project (SolidFrame)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Project version
#-----------------------------------------------------------------
file (READ "${CMAKE_SOURCE_DIR}/VERSION" VERSION)
string (REPLACE "\n" "" VERSION ${VERSION})
string (REPLACE "\r" "" VERSION ${VERSION})
string (REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)" "\\1" VERSION_MAJOR ${VERSION})
string (REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)" "\\2" VERSION_MINOR ${VERSION})
string (REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)" "\\3" VERSION_PATCH ${VERSION})

if(EXISTS ${CMAKE_SOURCE_DIR}/.svn)
	find_package(Subversion)
	if(Subversion_FOUND)
		Subversion_WC_INFO(${PROJECT_SOURCE_DIR} SVNINFO)
		message("-- Current SVN revision is ${SVNINFO_WC_REVISION}")
		set(VERSION_PATCH ${SVNINFO_WC_REVISION})
	else(Subversion_FOUND)
		message("")
		message(AUTHOR_WARNING "Could not determine SVN revision -- assumming ${VERSION_PATCH}")
		message("")
	endif(Subversion_FOUND)
else(EXISTS ${CMAKE_SOURCE_DIR}/.svn)
	message("")
	message(AUTHOR_WARNING "Could not determine SVN revision -- assumming ${VERSION_PATCH}")
	message("")
endif(EXISTS ${CMAKE_SOURCE_DIR}/.svn)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Extra user definitions
#-----------------------------------------------------------------
message("UDEFS set as "${UDEFS})
message("UEXTERN set as "${UEXTERN})
message("UEXTERN_ABS set as "${UEXTERN_ABS})


set(EXTRA_DEFINITIONS "${UDEFS}" CACHE STRING "Extra compiler definitions")


add_definitions(${EXTRA_DEFINITIONS})
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Prepare the extern path
#-----------------------------------------------------------------
string(LENGTH "${UEXTERN_ABS}" ExtAbsLen)
set(EXTERN_PATH)
if(ExtAbsLen GREATER 0)
	set(EXTERN_PATH "${UEXTERN_ABS}/" CACHE STRING "Extern dependencies path")
else(ExtAbsLen GREATER 0)
	string(LENGTH "${UEXTERN}" ExtLen)
	if(ExtLen GREATER 0)
		set(EXTERN_PATH "${SolidFrame_SOURCE_DIR}/extern/${UEXTERN}/" CACHE STRING "Extern dependencies path")
	else(ExtLen GREATER 0)
		set(EXTERN_PATH "${SolidFrame_SOURCE_DIR}/extern/" CACHE STRING "Extern dependencies path")
	endif(ExtLen GREATER 0)
endif(ExtAbsLen GREATER 0)

message("EXTERN_PATH set as "${EXTERN_PATH})

#-----------------------------------------------------------------
# Prepare the definitions for build types
#-----------------------------------------------------------------

string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE)

if(CMAKE_BUILD_TYPE STREQUAL "maintain")
	set(CMAKE_VERBOSE_MAKEFILE ON)
endif(CMAKE_BUILD_TYPE STREQUAL "maintain")


if(CMAKE_BUILD_TYPE STREQUAL "debug")
	set(CMAKE_VERBOSE_MAKEFILE ON)
endif(CMAKE_BUILD_TYPE STREQUAL "debug")

set(SF_VERSION_FLAGS "-DSF_MAJOR=${VERSION_MAJOR} -DSF_MINOR=${VERSION_MINOR} -DSF_PATCH=${VERSION_PATCH}")

set(SF_DEBUG_FLAGS "-DUDEBUG -DUASSERT -DNINLINES -DUSTATISTICS")
set(SF_MAINTAIN_FLAGS "-DUDEBUG -DUASSERT -DUSTATISTICS")
set(SF_OPTIMIZED_FLAGS "-DNINLINES")
set(SF_NOLOG_FLAGS "-DUASSERT -DNINLINES")
set(SF_RELEASE_FLAGS "")

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
message("cmake's system: ${CMAKE_SYSTEM}")
message("")

set(ON_FOUND FALSE)
set(ON_FREEBSD FALSE)
set(ON_MACOS FALSE)
set(ON_LINUX FALSE)
set(ON_POSIX FALSE)
set(ON_SOLARIS FALSE)

if(CMAKE_SYSTEM MATCHES "SunOS*")
	message("Detected system ON_SOLARIS")
	set(ON_FOUND TRUE)
	set(ON_SOLARIS TRUE)
	set(ON_POSIX TRUE)
	set(SYS_DEBUG_FLAGS "-DON_SOLARIS -DON_POSIX -features=extensions -library=stlport4 -mt -g") 
	set(SYS_MAINTAIN_FLAGS "-DON_SOLARIS -DON_POSIX -features=extensions -library=stlport4 -mt")
	set(SYS_OPTIMIZED_FLAGS "-DON_SOLARIS -DON_POSIX -features=extensions -library=stlport4 -mt")
	set(SYS_NOLOG_FLAGS "-DON_SOLARIS -DON_POSIX -features=extensions -library=stlport4 -mt")
	set(SYS_RELEASE_FLAGS "-DON_SOLARIS -DON_POSIX -features=extensions -library=stlport4 -mt")
	
	set(SYS_BASIC_LIBS pthread socket nsl)	

endif(CMAKE_SYSTEM MATCHES "SunOS*")

if(CMAKE_SYSTEM MATCHES "FreeBSD*")
	message("Detected system ON_FREEBSD")
	set(ON_FOUND TRUE)
	set(ON_FREEBSD TRUE)
	set(ON_POSIX TRUE)
	set(SYS_DEBUG_FLAGS "-DON_FREEBSD -DON_POSIX -Wreorder -Wreturn-type -Wunused-variable -Winline -g3")
	set(SYS_MAINTAIN_FLAGS "-DON_FREEBSD -DON_POSIX -Wall -Wabi -O3 -g3")
	set(SYS_OPTIMIZED_FLAGS "-DON_FREEBSD -DON_POSIX -O3 -g3")
	set(SYS_NOLOG_FLAGS "-DON_FREEBSD -DON_POSIX -g3")
	set(SYS_RELEASE_FLAGS "-DON_FREEBSD -DON_POSIX -O3")

	set(SYS_DEBUG_LINKER_FLAGS "")
	set(SYS_MAINTAIN_LINKER_FLAGS "-Wl,--warn-unresolved-symbols,--warn-once")
	set(SYS_OPTIMIZED_LINKER_FLAGS "")
	set(SYS_NOLOG_LINKER_FLAGS "")
	set(SYS_RELEASE_LINKER_FLAGS "")

	set(SYS_BASIC_LIBS pmc pthread rt)
endif(CMAKE_SYSTEM MATCHES "FreeBSD*")

if(CMAKE_SYSTEM MATCHES "Darwin*")
	message("Detected system ON_MACOS")
	set(ON_FOUND TRUE)
	set(ON_MACOS TRUE)
	set(ON_POSIX TRUE)
	set(SYS_DEBUG_FLAGS "-DON_MACOS -DON_POSIX -Wreorder -Wreturn-type -Wunused-variable -Winline -g3")
	set(SYS_MAINTAIN_FLAGS "-DON_MACOS -DON_POSIX -Wall -Wabi -O3 -g3")
	set(SYS_OPTIMIZED_FLAGS "-DON_MACOS -DON_POSIX -O3 -g3")
	set(SYS_NOLOG_FLAGS "-DON_MACOS -DON_POSIX -g3")
	set(SYS_RELEASE_FLAGS "-DON_MACOS -DON_POSIX -O3")

	set(SYS_DEBUG_LINKER_FLAGS "-framework ApplicationServices -framework CoreFoundation")
	set(SYS_MAINTAIN_LINKER_FLAGS "-Wl,--warn-unresolved-symbols,--warn-once -framework ApplicationServices -framework CoreFoundation")
	set(SYS_OPTIMIZED_LINKER_FLAGS "-framework ApplicationServices -framework CoreFoundation")
	set(SYS_NOLOG_LINKER_FLAGS "-framework ApplicationServices -framework CoreFoundation")
	set(SYS_RELEASE_LINKER_FLAGS "-framework ApplicationServices -framework CoreFoundation")

	set(SYS_BASIC_LIBS pthread)
endif(CMAKE_SYSTEM MATCHES "Darwin*")

if(CMAKE_SYSTEM MATCHES "Linux*")
	message("Detected system ON_LINUX")
	set(ON_FOUND TRUE)
	set(ON_LINUX TRUE)
	set(ON_POSIX TRUE)
	
	
	set(SYS_DEBUG_FLAGS "-DON_LINUX -DON_POSIX -Wreorder -Wreturn-type -Wunused-variable -Winline -g3")# -D_GLIBCXX_DEBUG
	set(SYS_MAINTAIN_FLAGS "-DON_LINUX -DON_POSIX -Wall -Wabi -O3 -g3")
	set(SYS_OPTIMIZED_FLAGS "-DON_LINUX -DON_POSIX -O3 -g3")
	set(SYS_NOLOG_FLAGS "-DON_LINUX -DON_POSIX -g3")
	set(SYS_RELEASE_FLAGS "-DON_LINUX -DON_POSIX -O3")

	set(SYS_DEBUG_LINKER_FLAGS "")# -D_GLIBCXX_DEBUG
	set(SYS_MAINTAIN_LINKER_FLAGS "-Wl,--warn-unresolved-symbols,--warn-once")
	set(SYS_OPTIMIZED_LINKER_FLAGS "")
	set(SYS_NOLOG_LINKER_FLAGS "")
	set(SYS_RELEASE_LINKER_FLAGS "")
	
	set(SYS_BASIC_LIBS pthread rt)
	set(SYS_DYNAMIC_LOAD_LIB dl)
endif(CMAKE_SYSTEM MATCHES "Linux*")

if(CMAKE_SYSTEM MATCHES "Windows*")
	message("Detected system ON_WINDOWS")
	set(ON_FOUND TRUE)
	set(ON_WINDOWS TRUE)
	set(SYS_DEBUG_FLAGS "-DON_WINDOWS -D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0501")
	set(SYS_MAINTAIN_FLAGS "-DON_WINDOWS -D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0501")
	set(SYS_OPTIMIZED_FLAGS "-DON_WINDOWS -D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0501")
	set(SYS_NOLOG_FLAGS "-DON_WINDOWS -D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0501")
	set(SYS_RELEASE_FLAGS "-DON_WINDOWS -D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0501")

	set(SYS_DEBUG_LINKER_FLAGS "")
	set(SYS_MAINTAIN_LINKER_FLAGS "")
	set(SYS_OPTIMIZED_LINKER_FLAGS "")
	set(SYS_NOLOG_LINKER_FLAGS "")
	set(SYS_RELEASE_LINKER_FLAGS "")
	
endif(CMAKE_SYSTEM MATCHES "Windows*")

message("")

if(NOT ON_FOUND)
	message("\r\n === Sorry unsupported system - please contact project owner ===\r\n")
	exit()#only to raise an exception
	return()
endif(NOT ON_FOUND)


#-----------------------------------------------------------------
# config.h support
#-----------------------------------------------------------------
include(check.config.cmake)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

if(HAVE_CPP11FLAG AND HAVE_CPP11)
	add_definitions(-std=c++0x)
endif(HAVE_CPP11FLAG AND HAVE_CPP11)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Debug build
#-----------------------------------------------------------------
set(CMAKE_CXX_FLAGS_DEBUG 
	"${SYS_DEBUG_FLAGS} ${SF_DEBUG_FLAGS} ${SF_VERSION_FLAGS}"
	CACHE STRING "Flags used by the C++ compiler during debug builds."
	FORCE
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Maintain build
#-----------------------------------------------------------------
set(CMAKE_CXX_FLAGS_MAINTAIN 
	"${SYS_MAINTAIN_FLAGS} ${SF_MAINTAIN_FLAGS} ${SF_VERSION_FLAGS}"
	CACHE STRING "Flags used by the C++ compiler during maintain builds."
	FORCE
)
#set(CMAKE_C_FLAGS_MAINTAIN "" CACHE STRING "Flags used by the C compiler during maintain builds." FORCE)
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
	"${SYS_OPTIMIZED_FLAGS} ${SF_OPTIMIZED_FLAGS} ${SF_VERSION_FLAGS}"
	CACHE STRING "Flags used by the C++ compiler during optimized builds."
	FORCE
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Release build
#-----------------------------------------------------------------
set(CMAKE_CXX_FLAGS_RELEASE
	"${SYS_RELEASE_FLAGS} ${SF_RELEASE_FLAGS} ${SF_VERSION_FLAGS}"
	CACHE STRING "Flags used by the C++ compiler during release builds."
	FORCE
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Nolog build
#-----------------------------------------------------------------
set(CMAKE_CXX_FLAGS_NOLOG 
	"${SYS_NOLOG_FLAGS} ${SF_NOLOG_FLAGS} ${SF_VERSION_FLAGS}"
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
# Find libraries
#-----------------------------------------------------------------

include(find_boost.cmake)
include(find_openssl.cmake)

#-----------------------------------------------------------------
# Global include directories
#-----------------------------------------------------------------
include_directories(
	${SolidFrame_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${SolidFrame_SOURCE_DIR}/library
	${SolidFrame_SOURCE_DIR}/application
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
if(UTESTING)
	INCLUDE(CTest)
	ENABLE_TESTING()
endif(UTESTING)

#-----------------------------------------------------------------
# Project subdirectories
#-----------------------------------------------------------------
add_subdirectory (system)
add_subdirectory (utility)
add_subdirectory (algorithm)
add_subdirectory (foundation)
add_subdirectory (audit)
add_subdirectory (test)
add_subdirectory (example)
add_subdirectory (library)
add_subdirectory (application)
add_subdirectory (distributed)
#-----------------------------------------------------------------
