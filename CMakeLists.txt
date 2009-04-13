cmake_minimum_required(VERSION 2.4)
#-----------------------------------------------------------------
#the project
#-----------------------------------------------------------------
project (SOLIDGROUND)

#-----------------------------------------------------------------
# project version
#-----------------------------------------------------------------
file (READ "${CMAKE_SOURCE_DIR}/VERSION" VERSION)
string (REPLACE "\n" "" VERSION ${VERSION})
string (REPLACE "\r" "" VERSION ${VERSION})
string (REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)" "\\1" VERSION_MAJOR ${VERSION})
string (REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)" "\\2" VERSION_MINOR ${VERSION})
string (REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)" "\\3" VERSION_PATCH ${VERSION})


#-----------------------------------------------------------------
# extra user definitions
#-----------------------------------------------------------------
message("UDEFS set "${UDEFS})
message("UEXTERN set "${UEXTERN})
message("UEXTERN_ABS set "${UEXTERN_ABS})
ADD_DEFINITIONS(${UDEFS})

#-----------------------------------------------------------------
# prepare the extern path
#-----------------------------------------------------------------

STRING(LENGTH "${UEXTERN_ABS}" ExtAbsLen)
SET(EXTERN_PATH)
IF(ExtAbsLen GREATER 0)
	SET(EXTERN_PATH "${UEXTERN_ABS}/")
ELSE(ExtAbsLen GREATER 0)
	STRING(LENGTH ${UEXTERN} ExtLen)
	IF(ExtLen GREATER 0)
		SET(EXTERN_PATH "${SOLIDGROUND_SOURCE_DIR}/extern/${UEXTERN}/")
	ELSE(ExtLen GREATER 0)
		SET(EXTERN_PATH "${SOLIDGROUND_SOURCE_DIR}/extern/")
	ENDIF(ExtLen GREATER 0)
ENDIF(ExtAbsLen GREATER 0)
MESSAGE("EXTERN_PATH set as "${EXTERN_PATH})
#-----------------------------------------------------------------
# prepare the definitions for build types
#-----------------------------------------------------------------

IF(CMAKE_BUILD_TYPE STREQUAL "maintain")
	SET(CMAKE_VERBOSE_MAKEFILE ON)
ENDIF(CMAKE_BUILD_TYPE STREQUAL "maintain")

IF(CMAKE_BUILD_TYPE STREQUAL "debug")
	SET(CMAKE_VERBOSE_MAKEFILE ON)
ENDIF(CMAKE_BUILD_TYPE STREQUAL "debug")

SET(SG_VERSION_FLAGS "-DSG_MAJOR=${VERSION_MAJOR} -DSG_MINOR=${VERSION_MINOR} -DSG_PATCH=${VERSION_PATCH}")

SET(SG_DEBUG_FLAGS "-DUDEBUG -DUTHREADS -DUASSERT -DU_SERIALIZATION_MUTEX")
SET(SG_MAINTAIN_FLAGS "-DUDEBUG -DUINLINES -DUASSERT -DUTHREADS -DU_SERIALIZATION_MUTEX")
SET(SG_OPTIMIZED_FLAGS "-DUTHREADS -DU_SERIALIZATION_MUTEX -DUASSERT")
SET(SG_NOLOG_FLAGS "-DUTHREADS -DUASSERT -DU_SERIALIZATION_MUTEX")
SET(SG_RELEASE_FLAGS "-DUTHREADS -DUINLINES -DU_SERIALIZATION_MUTEX")

SET(SYS_DEBUG_FLAGS "")
SET(SYS_MAINTAIN_FLAGS "")
SET(SYS_OPTIMIZED_FLAGS "")
SET(SYS_NOLOG_FLAGS "")
SET(SYS_RELEASE_FLAGS "")

SET(SYS_DEBUG_LINKER_FLAGS "")
SET(SYS_MAINTAIN_LINKER_FLAGS "")
SET(SYS_OPTIMIZED_LINKER_FLAGS "")
SET(SYS_NOLOG_LINKER_FLAGS "")
SET(SYS_RELEASE_LINKER_FLAGS "")

SET(SYS_BASIC_LIBS "")

IF(CMAKE_SYSTEM MATCHES "SunOS.*")
	SET(SYS_DEBUG_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -DON_SUN -features=extensions") 
	SET(SYS_MAINTAIN_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -DON_SUN")
	SET(SYS_OPTIMIZED_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} -DON_SUN")
	SET(SYS_NOLOG_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -DON_SUN")
	SET(SYS_RELEASE_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} -DON_SUN")
	
	SET(SYS_BASIC_LIBS pthread socket)	

ELSE(CMAKE_SYSTEM MATCHES "SunOS.*")
	SET(SYS_DEBUG_FLAGS "-Wreorder -Wreturn-type -Wunused-variable -Winline -g3")
	SET(SYS_MAINTAIN_FLAGS "-Wall -Wabi -O3 -g3")
	SET(SYS_OPTIMIZED_FLAGS "-O3 -g3")
	SET(SYS_NOLOG_FLAGS "-g3")
	SET(SYS_RELEASE_FLAGS "-O3")

	SET(SYS_DEBUG_LINKER_FLAGS "")
	SET(SYS_MAINTAIN_LINKER_FLAGS "-Wl,--warn-unresolved-symbols,--warn-once")
	SET(SYS_OPTIMIZED_LINKER_FLAGS "")
	SET(SYS_NOLOG_LINKER_FLAGS "")
	SET(SYS_RELEASE_LINKER_FLAGS "")

	SET(SYS_BASIC_LIBS pthread rt)
ENDIF(CMAKE_SYSTEM MATCHES "SunOS.*")

#-----------------------------------------------------------------
#debug build
#-----------------------------------------------------------------
SET(CMAKE_CXX_FLAGS_DEBUG 
	"${SYS_DEBUG_FLAGS} ${SG_DEBUG_FLAGS} ${SG_VERSION_FLAGS}"
	CACHE STRING "Flags used by the C++ compiler during debug builds."
	FORCE
)
#-----------------------------------------------------------------
#maintain build
#-----------------------------------------------------------------
SET(CMAKE_CXX_FLAGS_MAINTAIN 
	"${SYS_MAINTAIN_FLAGS} ${SG_MAINTAIN_FLAGS} ${SG_VERSION_FLAGS}"
	CACHE STRING "Flags used by the C++ compiler during maintain builds."
	FORCE
)
#SET(CMAKE_C_FLAGS_MAINTAIN "" CACHE STRING "Flags used by the C compiler during maintain builds." FORCE)
SET(CMAKE_EXE_LINKER_FLAGS_MAINTAIN
    "${SYS_MAINTAIN_LINKER_FLAGS}"
	CACHE STRING "Flags used for linking binaries during maintain builds."
    FORCE
)
SET(CMAKE_SHARED_LINKER_FLAGS_MAINTAIN
    "${SYS_MAINTAIN_LINKER_FLAGS}"
	CACHE STRING "Flags used by the shared libraries linker during maintain builds."
    FORCE
)

#-----------------------------------------------------------------
#optimized build
#-----------------------------------------------------------------
SET(CMAKE_CXX_FLAGS_OPTIMIZED 
	"${SYS_OPTIMIZED_FLAGS} ${SG_OPTIMIZED_FLAGS} ${SG_VERSION_FLAGS}"
	CACHE STRING "Flags used by the C++ compiler during optimized builds."
	FORCE
)
#-----------------------------------------------------------------
#release build
#-----------------------------------------------------------------
SET(CMAKE_CXX_FLAGS_RELEASE
	"${SYS_RELEASE_FLAGS} ${SG_RELEASE_FLAGS} ${SG_VERSION_FLAGS}"
	CACHE STRING "Flags used by the C++ compiler during release builds."
	FORCE
)

#-----------------------------------------------------------------
#nolog build
#-----------------------------------------------------------------
SET(CMAKE_CXX_FLAGS_NOLOG 
	"${SYS_NOLOG_FLAGS} ${SG_NOLOG_FLAGS} ${SG_VERSION_FLAGS}"
	CACHE STRING "Flags used by the C++ compiler during nolog builds."
    FORCE
)

#-----------------------------------------------------------------
MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_NOLOG
    CMAKE_C_FLAGS_NOLOG
    CMAKE_EXE_LINKER_FLAGS_NOLOG
    CMAKE_SHARED_LINKER_FLAGS_NOLOG
)
#-----------------------------------------------------------------
MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_MAINTAIN
    CMAKE_C_FLAGS_MAINTAIN
    CMAKE_EXE_LINKER_FLAGS_MAINTAIN
    CMAKE_SHARED_LINKER_FLAGS_MAINTAIN
)
#-----------------------------------------------------------------
MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_OPTIMIZED
    CMAKE_C_FLAGS_OPTIMIZED
    CMAKE_EXE_LINKER_FLAGS_OPTIMIZED
    CMAKE_SHARED_LINKER_FLAGS_OPTIMIZED
)
#-----------------------------------------------------------------

#-----------------------------------------------------------------
# Update the documentation string of CMAKE_BUILD_TYPE for GUIs
#-----------------------------------------------------------------
SET(CMAKE_BUILD_TYPE
	"${CMAKE_BUILD_TYPE}"
	CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel nolog maintain optimized:"
    FORCE
)
#-----------------------------------------------------------------

include_directories(
	${SOLIDGROUND_SOURCE_DIR}
	${SOLIDGROUND_SOURCE_DIR}/library
	${EXTERN_PATH}/include/openssl
	${EXTERN_PATH}/include/tclap
	${EXTERN_PATH}/include/
)

link_directories(
	${EXTERN_PATH}/lib
)

add_subdirectory (system)
add_subdirectory (utility)
add_subdirectory (algorithm)
add_subdirectory (foundation)
add_subdirectory (audit)
add_subdirectory (test)
add_subdirectory (library)
add_subdirectory (application)
