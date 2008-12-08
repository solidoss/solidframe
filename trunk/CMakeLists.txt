#the project
project (SOLIDGROUND)

file (READ "${CMAKE_SOURCE_DIR}/VERSION" VERSION)
string (REPLACE "\n" "" VERSION ${VERSION})
string (REPLACE "\r" "" VERSION ${VERSION})
string (REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)" "\\1" VERSION_MAJOR ${VERSION})
string (REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)" "\\2" VERSION_MINOR ${VERSION})
string (REGEX REPLACE "^([0-9]*)\\.([0-9]*)\\.([0-9]*)" "\\3" VERSION_PATCH ${VERSION})

SET(CMAKE_CXX_FLAGS_DEBUG "-Wreorder -Wreturn-type -Wunused-variable -Winline -g3 -DUDEBUG -DUTHREADS -DUASSERT -DU_SERIALIZATION_MUTEX -DSG_MAJOR=${VERSION_MAJOR} -DSG_MINOR=${VERSION_MINOR} -DSG_PATCH=${VERSION_PATCH}" CACHE STRING "Flags used by the C++ compiler during debug builds." FORCE)
SET(CMAKE_C_FLAGS_DEBUG "-g3  -DUASSERT -DSG_MAJOR=${VERSION_MAJOR} -DSG_MINOR=${VERSION_MINOR} -DSG_PATCH=${VERSION_PATCH}" CACHE STRING "Flags used by the C compiler during debug builds." FORCE)
SET( CMAKE_EXE_LINKER_FLAGS_DEBUG
    "" CACHE STRING
    "Flags used for linking binaries during debug builds."
    FORCE )
SET( CMAKE_SHARED_LINKER_FLAGS_DEBUG
    "" CACHE STRING
    "Flags used by the shared libraries linker during debug builds."
    FORCE )

SET(CMAKE_CXX_FLAGS_MAINTAIN "-Wall -Wabi -O3 -g3 -DUDEBUG -DUINLINES -DUASSERT -DUTHREADS -DU_SERIALIZATION_MUTEX -DSG_MAJOR=${VERSION_MAJOR} -DSG_MINOR=${VERSION_MINOR} -DSG_PATCH=${VERSION_PATCH}" CACHE STRING "Flags used by the C++ compiler during maintain builds." FORCE)
SET(CMAKE_C_FLAGS_MAINTAIN "-Wall -pedantic -g3 -DSG_MAJOR=${VERSION_MAJOR} -DSG_MINOR=${VERSION_MINOR} -DSG_PATCH=${VERSION_PATCH}" CACHE STRING "Flags used by the C compiler during maintain builds." FORCE)
SET( CMAKE_EXE_LINKER_FLAGS_MAINTAIN
    "-Wl,--warn-unresolved-symbols,--warn-once" CACHE STRING
    "Flags used for linking binaries during maintain builds."
    FORCE )
SET( CMAKE_SHARED_LINKER_FLAGS_MAINTAIN
    "-Wl,--warn-unresolved-symbols,--warn-once" CACHE STRING
    "Flags used by the shared libraries linker during maintain builds."
    FORCE )

SET(CMAKE_CXX_FLAGS_OPTIMIZED "-O3 -g3 -DUTHREADS -DU_SERIALIZATION_MUTEX -DUASSERT -DSG_MAJOR=${VERSION_MAJOR} -DSG_MINOR=${VERSION_MINOR} -DSG_PATCH=${VERSION_PATCH}" CACHE STRING "Flags used by the C++ compiler during optimized builds." FORCE)
SET(CMAKE_C_FLAGS_OPTIMIZED "-O3 -g3 -DSG_MAJOR=${VERSION_MAJOR} -DSG_MINOR=${VERSION_MINOR} -DSG_PATCH=${VERSION_PATCH}" CACHE STRING "Flags used by the C compiler during optimized builds." FORCE)
SET( CMAKE_EXE_LINKER_FLAGS_OPTIMIZED
    "" CACHE STRING
    "Flags used for linking binaries during optimized builds."
    FORCE )
SET( CMAKE_SHARED_LINKER_FLAGS_OPTIMIZED
    "" CACHE STRING
    "Flags used by the shared libraries linker during optimized builds."
    FORCE )


#overwrite the defaults for release
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -DUTHREADS -DUINLINES -DUASSERT -DU_SERIALIZATION_MUTEX -Wuninitialized -DSG_MAJOR=${VERSION_MAJOR} -DSG_MINOR=${VERSION_MINOR} -DSG_PATCH=${VERSION_PATCH}" CACHE STRING "Flags used by the C++ compiler during release builds." FORCE)
#SET(CMAKE_C_FLAGS_RELEASE "-O3 -DUTHREADS" CACHE STRING "Flags used by the C compiler during release builds." FORCE)


#nolog - debug without log
SET( CMAKE_CXX_FLAGS_NOLOG "-g3 -DUTHREADS -DUASSERT -DU_SERIALIZATION_MUTEX -DSG_MAJOR=${VERSION_MAJOR} -DSG_MINOR=${VERSION_MINOR} -DSG_PATCH=${VERSION_PATCH}" CACHE STRING
    "Flags used by the C++ compiler during nolog builds."
    FORCE )
SET( CMAKE_C_FLAGS_NOLOG "-g3 -DUTHREADS -DUASSERT -DU_SERIALIZATION_MUTEX -DSG_MAJOR=${VERSION_MAJOR} -DSG_MINOR=${VERSION_MINOR} -DSG_PATCH=${VERSION_PATCH}" CACHE STRING
    "Flags used by the C compiler during nolog builds."
    FORCE )
SET( CMAKE_EXE_LINKER_FLAGS_NOLOG
    "" CACHE STRING
    "Flags used for linking binaries during nolog builds."
    FORCE )
SET( CMAKE_SHARED_LINKER_FLAGS_NOLOG
    "" CACHE STRING
    "Flags used by the shared libraries linker during xscale builds."
    FORCE )

MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_NOLOG
    CMAKE_C_FLAGS_NOLOG
    CMAKE_EXE_LINKER_FLAGS_NOLOG
    CMAKE_SHARED_LINKER_FLAGS_NOLOG )

MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_MAINTAIN
    CMAKE_C_FLAGS_MAINTAIN
    CMAKE_EXE_LINKER_FLAGS_MAINTAIN
    CMAKE_SHARED_LINKER_FLAGS_MAINTAIN )

MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_OPTIMIZED
    CMAKE_C_FLAGS_OPTIMIZED
    CMAKE_EXE_LINKER_FLAGS_OPTIMIZED
    CMAKE_SHARED_LINKER_FLAGS_OPTIMIZED )

# Update the documentation string of CMAKE_BUILD_TYPE for GUIs
SET( CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING
    "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel nolog maintain optimized:"
    FORCE )
    
include_directories(
	${SOLIDGROUND_SOURCE_DIR}
	${SOLIDGROUND_SOURCE_DIR}/extern/linux/openssl/include
	${SOLIDGROUND_SOURCE_DIR}/extern/linux/tclap/include
)

link_directories(
	${SOLIDGROUND_SOURCE_DIR}/system
	${SOLIDGROUND_SOURCE_DIR}/extern/linux/openssl
)

add_subdirectory (system)
add_subdirectory (utility)
add_subdirectory (algorithm)
add_subdirectory (clientserver)
add_subdirectory (test)
add_subdirectory (application)
add_subdirectory (audit)
