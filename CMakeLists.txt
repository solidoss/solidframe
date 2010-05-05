cmake_minimum_required(VERSION 2.6)
#-----------------------------------------------------------------
# The project
#-----------------------------------------------------------------
project (SOLIDGROUND)
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
		set(EXTERN_PATH "${SOLIDGROUND_SOURCE_DIR}/extern/${UEXTERN}/"  CACHE STRING "Extern dependencies path")
	else(ExtLen GREATER 0)
		set(EXTERN_PATH "${SOLIDGROUND_SOURCE_DIR}/extern/"  CACHE STRING "Extern dependencies path")
	endif(ExtLen GREATER 0)
endif(ExtAbsLen GREATER 0)

message("EXTERN_PATH set as "${EXTERN_PATH})
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Prepare the definitions for build types
#-----------------------------------------------------------------
if(CMAKE_BUILD_TYPE STREQUAL "maintain")
	set(CMAKE_VERBOSE_MAKEFILE ON)
endif(CMAKE_BUILD_TYPE STREQUAL "maintain")

if(CMAKE_BUILD_TYPE STREQUAL "debug")
	set(CMAKE_VERBOSE_MAKEFILE ON)
endif(CMAKE_BUILD_TYPE STREQUAL "debug")

set(SG_VERSION_FLAGS "-DSG_MAJOR=${VERSION_MAJOR} -DSG_MINOR=${VERSION_MINOR} -DSG_PATCH=${VERSION_PATCH}")

set(SG_DEBUG_FLAGS "-DUDEBUG -DUASSERT -DNINLINES -DUSTATISTICS")
set(SG_MAINTAIN_FLAGS "-DUDEBUG -DUASSERT -DUSTATISTICS")
set(SG_OPTIMIZED_FLAGS "-DNINLINES")
set(SG_NOLOG_FLAGS "-DUASSERT -DNINLINES")
set(SG_RELEASE_FLAGS "")

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

set(ON_SUN FALSE)

message("")
message("cmake's system: ${CMAKE_SYSTEM}")
message("")

set(ON_FOUND FALSE)
set(ON_FBSD FALSE)
set(ON_MACOS FALSE)
set(ON_LINUX FALSE)
set(ON_POSIX FALSE)

if(CMAKE_SYSTEM MATCHES "SunOS*")
	message("Detected system ON_SUN")
	set(ON_FOUND TRUE)
	#set(ON_SUN TRUE)
	set(SYS_DEBUG_FLAGS "-DON_SUN -features=extensions -library=stlport4 -mt -g") 
	set(SYS_MAINTAIN_FLAGS "-DON_SUN -features=extensions -library=stlport4 -mt")
	set(SYS_OPTIMIZED_FLAGS "-DON_SUN -features=extensions -library=stlport4 -mt")
	set(SYS_NOLOG_FLAGS "-DON_SUN -features=extensions -library=stlport4 -mt")
	set(SYS_RELEASE_FLAGS "-DON_SUN -features=extensions -library=stlport4 -mt")
	
	set(SYS_BASIC_LIBS pthread socket nsl)	

endif(CMAKE_SYSTEM MATCHES "SunOS*")

if(CMAKE_SYSTEM MATCHES "FreeBSD*")
	message("Detected system ON_FBSD")
	set(ON_FOUND TRUE)
	set(SYS_DEBUG_FLAGS "-DON_FBSD -Wreorder -Wreturn-type -Wunused-variable -Winline -g3")
	set(SYS_MAINTAIN_FLAGS "-DON_FBSD -Wall -Wabi -O3 -g3")
	set(SYS_OPTIMIZED_FLAGS "-DON_FBSD -O3 -g3")
	set(SYS_NOLOG_FLAGS "-DON_FBSD -g3")
	set(SYS_RELEASE_FLAGS "-DON_FBSD -O3")

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
	set(SYS_DEBUG_FLAGS "-DON_MACOS -Wreorder -Wreturn-type -Wunused-variable -Winline -g3")
	set(SYS_MAINTAIN_FLAGS "-DON_MACOS -Wall -Wabi -O3 -g3")
	set(SYS_OPTIMIZED_FLAGS "-DON_MACOS -O3 -g3")
	set(SYS_NOLOG_FLAGS "-DON_MACOS -g3")
	set(SYS_RELEASE_FLAGS "-DON_MACOS -O3")

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
	
	set(SYS_BASIC_LIBS pthread rt)
endif(CMAKE_SYSTEM MATCHES "Linux*")

if(CMAKE_SYSTEM MATCHES "Windows*")
	message("Detected system ON_WINDOWS")
	set(ON_FOUND TRUE)
	set(SYS_DEBUG_FLAGS "-D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0501")
	set(SYS_MAINTAIN_FLAGS "-D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0501")
	set(SYS_OPTIMIZED_FLAGS "-D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0501")
	set(SYS_NOLOG_FLAGS "-D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0501")
	set(SYS_RELEASE_FLAGS "-D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0501")

	set(SYS_DEBUG_LINKER_FLAGS "")
	set(SYS_MAINTAIN_LINKER_FLAGS "-Wl,--warn-unresolved-symbols,--warn-once")
	set(SYS_OPTIMIZED_LINKER_FLAGS "")
	set(SYS_NOLOG_LINKER_FLAGS "")
	set(SYS_RELEASE_LINKER_FLAGS "")
	
	set(SYS_BASIC_LIBS pthread rt)
endif(CMAKE_SYSTEM MATCHES "Windows*")

message("")

if(NOT ON_FOUND)
	message("\r\n === Sorry unsupported system - please contact project owner ===\r\n")
	exit()#only to raise an exception
	return()
endif(NOT ON_FOUND)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Debug build
#-----------------------------------------------------------------
set(CMAKE_CXX_FLAGS_DEBUG 
	"${SYS_DEBUG_FLAGS} ${SG_DEBUG_FLAGS} ${SG_VERSION_FLAGS}"
	CACHE STRING "Flags used by the C++ compiler during debug builds."
	FORCE
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Maintain build
#-----------------------------------------------------------------
set(CMAKE_CXX_FLAGS_MAINTAIN 
	"${SYS_MAINTAIN_FLAGS} ${SG_MAINTAIN_FLAGS} ${SG_VERSION_FLAGS}"
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
	"${SYS_OPTIMIZED_FLAGS} ${SG_OPTIMIZED_FLAGS} ${SG_VERSION_FLAGS}"
	CACHE STRING "Flags used by the C++ compiler during optimized builds."
	FORCE
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Release build
#-----------------------------------------------------------------
set(CMAKE_CXX_FLAGS_RELEASE
	"${SYS_RELEASE_FLAGS} ${SG_RELEASE_FLAGS} ${SG_VERSION_FLAGS}"
	CACHE STRING "Flags used by the C++ compiler during release builds."
	FORCE
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Nolog build
#-----------------------------------------------------------------
set(CMAKE_CXX_FLAGS_NOLOG 
	"${SYS_NOLOG_FLAGS} ${SG_NOLOG_FLAGS} ${SG_VERSION_FLAGS}"
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
	${SOLIDGROUND_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${SOLIDGROUND_SOURCE_DIR}/library
	${SOLIDGROUND_SOURCE_DIR}/application
	${EXTERN_PATH}/include/openssl
	${EXTERN_PATH}/include/tclap
	${EXTERN_PATH}/include/
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# config.h support
#-----------------------------------------------------------------
include(check.config.cmake)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

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
#-----------------------------------------------------------------
