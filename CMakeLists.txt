cmake_minimum_required(VERSION 2.4)
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

find_package(Subversion)

if(Subversion_FOUND)
	Subversion_WC_INFO(${PROJECT_SOURCE_DIR} SVNINFO)
	message("-- Current SVN revision is ${SVNINFO_WC_REVISION}")
	set(VERSION_PATCH ${SVNINFO_WC_REVISION})
else(Subversion_FOUND)
	message(SEND_ERROR "Could not determine SVN revision -- assumming ${VERSION_PATCH}")
endif(Subversion_FOUND)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Extra user definitions
#-----------------------------------------------------------------
message("UDEFS set "${UDEFS})
message("UEXTERN set "${UEXTERN})
message("UEXTERN_ABS set "${UEXTERN_ABS})
add_definitions(${UDEFS})
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Prepare the extern path
#-----------------------------------------------------------------
string(LENGTH "${UEXTERN_ABS}" ExtAbsLen)
set(EXTERN_PATH)
if(ExtAbsLen GREATER 0)
	set(EXTERN_PATH "${UEXTERN_ABS}/")
else(ExtAbsLen GREATER 0)
	string(LENGTH "${UEXTERN}" ExtLen)
	if(ExtLen GREATER 0)
		set(EXTERN_PATH "${SOLIDGROUND_SOURCE_DIR}/extern/${UEXTERN}/")
	else(ExtLen GREATER 0)
		set(EXTERN_PATH "${SOLIDGROUND_SOURCE_DIR}/extern/")
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

set(SG_DEBUG_FLAGS "-DUDEBUG -DUTHREADS -DUASSERT -DU_SERIALIZATION_MUTEX")
set(SG_MAINTAIN_FLAGS "-DUDEBUG -DUINLINES -DUASSERT -DUTHREADS -DU_SERIALIZATION_MUTEX")
set(SG_OPTIMIZED_FLAGS "-DUTHREADS -DU_SERIALIZATION_MUTEX -DUASSERT")
set(SG_NOLOG_FLAGS "-DUTHREADS -DUASSERT -DU_SERIALIZATION_MUTEX")
set(SG_RELEASE_FLAGS "-DUTHREADS -DUINLINES -DU_SERIALIZATION_MUTEX")

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

message("cmake's system: ${CMAKE_SYSTEM}")

set(ON_FOUND FALSE)

if(CMAKE_SYSTEM MATCHES "SunOS*")
	message("Detected system ON_SUN")
	set(ON_FOUND TRUE)
	#set(ON_SUN TRUE)
	set(SYS_DEBUG_FLAGS "-DON_SUN -features=extensions -library=stlport4 -g") 
	set(SYS_MAINTAIN_FLAGS "-DON_SUN")
	set(SYS_OPTIMIZED_FLAGS "-DON_SUN")
	set(SYS_NOLOG_FLAGS "-DON_SUN")
	set(SYS_RELEASE_FLAGS "-DON_SUN")
	
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

if(NOT ON_FOUND)
	message("Detected system ON_LINUX")
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
	${SOLIDGROUND_SOURCE_DIR}/library
	${SOLIDGROUND_SOURCE_DIR}/application
	${EXTERN_PATH}/include/openssl
	${EXTERN_PATH}/include/tclap
	${EXTERN_PATH}/include/
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Global link directories
#-----------------------------------------------------------------
link_directories(
	"."
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
