#the workspace project
project (SOLIDGROUND)

SET(CMAKE_CXX_FLAGS_DEBUG "-Wreorder -Wreturn-type -Wunused-variable -Winline -g3 -DUDEBUG -DUTHREADS -DUASSERT -DU_SERIALIZATION_MUTEX" CACHE STRING "Flags used by the C++ compiler during debug builds." FORCE)
SET(CMAKE_C_FLAGS_DEBUG "-g3  -DUASSERT" CACHE STRING "Flags used by the C compiler during debug builds." FORCE)
SET( CMAKE_EXE_LINKER_FLAGS_DEBUG
    "" CACHE STRING
    "Flags used for linking binaries during debug builds."
    FORCE )
SET( CMAKE_SHARED_LINKER_FLAGS_DEBUG
    "" CACHE STRING
    "Flags used by the shared libraries linker during debug builds."
    FORCE )

SET(CMAKE_CXX_FLAGS_MAINTAIN "-Wall -Wabi -g3 -DUDEBUG -DUINLINES -DUTHREADS -DU_SERIALIZATION_MUTEX" CACHE STRING "Flags used by the C++ compiler during maintain builds." FORCE)
SET(CMAKE_C_FLAGS_MAINTAIN "-Wall -pedantic -g3" CACHE STRING "Flags used by the C compiler during maintain builds." FORCE)
SET( CMAKE_EXE_LINKER_FLAGS_MAINTAIN
    "-Wl,--warn-unresolved-symbols,--warn-once" CACHE STRING
    "Flags used for linking binaries during maintain builds."
    FORCE )
SET( CMAKE_SHARED_LINKER_FLAGS_MAINTAIN
    "-Wl,--warn-unresolved-symbols,--warn-once" CACHE STRING
    "Flags used by the shared libraries linker during maintain builds."
    FORCE )


#overwrite the defaults for release
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -g3 -DUTHREADS -DUINLINES -DU_SERIALIZATION_MUTEX -Wuninitialized" CACHE STRING "Flags used by the C++ compiler during release builds." FORCE)
#SET(CMAKE_C_FLAGS_RELEASE "-O3 -DUTHREADS" CACHE STRING "Flags used by the C compiler during release builds." FORCE)


#nolog - debug without log
SET( CMAKE_CXX_FLAGS_NOLOG "-g3 -DUTHREADS -DUASSERT -DU_SERIALIZATION_MUTEX" CACHE STRING
    "Flags used by the C++ compiler during nolog builds."
    FORCE )
SET( CMAKE_C_FLAGS_NOLOG "-g3 -DUTHREADS -DUASSERT -DU_SERIALIZATION_MUTEX" CACHE STRING
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

# Update the documentation string of CMAKE_BUILD_TYPE for GUIs
SET( CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING
    "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel nolog maintain:"
    FORCE )

#new xscale build
# SET( CMAKE_CXX_FLAGS_XSCALE "-O1 -mcpu=xscale -O2 -fomit-frame-pointer -D__USE_STRING_INLINES -pipe -D_POSIX_PATH_MAX=256 -DNDEBUG -funsigned-char -fno-strict-aliasing -I${TMCCM_SOURCE_DIR}/libsTmccm/log4c++/xscale/log4cpp-0.3.5rc3/include -DHIGHFIRST" CACHE STRING
#     "Flags used by the C++ compiler during xscale builds."
#     FORCE )
# SET( CMAKE_C_FLAGS_XSCALE "-O1 -mcpu=xscale -O2 -fomit-frame-pointer -D__USE_STRING_INLINES -pipe -D_POSIX_PATH_MAX=256 -DNDEBUG -funsigned-char -fno-strict-aliasing -I${TMCCM_SOURCE_DIR}/libsTmccm/log4c++/xscale/log4cpp-0.3.5rc3/include -DHIGHFIRST" CACHE STRING
#     "Flags used by the C compiler during xscale builds."
#     FORCE )
# SET( CMAKE_EXE_LINKER_FLAGS_XSCALE
#     "" CACHE STRING
#     "Flags used for linking binaries during xscale builds."
#     FORCE )
# SET( CMAKE_SHARED_LINKER_FLAGS_XSCALE
#     "" CACHE STRING
#     "Flags used by the shared libraries linker during xscale builds."
#     FORCE )
# 
# MARK_AS_ADVANCED(
#     CMAKE_CXX_FLAGS_XSCALE
#     CMAKE_C_FLAGS_XSCALE
#     CMAKE_EXE_LINKER_FLAGS_XSCALE
#     CMAKE_SHARED_LINKER_FLAGS_XSCALE )
# # Update the documentation string of CMAKE_BUILD_TYPE for GUIs
# SET( CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING
#     "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel XScale:"
#     FORCE )

#string (TOLOWER ${CMAKE_BUILD_TYPE} LOWER_BUILD_TYPE)

# if (CMAKE_BUILD_TYPE MATCHES "xscale")
# 	set (CMAKE_CXX_COMPILER "arm-wrs-linux-gnueabi-armv5teb-glibc_cgl-g++")
# 	set (CMAKE_C_COMPILER "arm-wrs-linux-gnueabi-armv5teb-glibc_cgl-gcc")
# 	set (CMAKE_AR "arm-wrs-linux-gnueabi-armv5teb-glibc_cgl-ar")
# 	set (CMAKE_RANLIB "arm-wrs-linux-gnueabi-armv5teb-glibc_cgl-ranlib")
# 	set (STRIP "arm-wrs-linux-gnueabi-armv5teb-glibc_cgl-strip")
# 	set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")
# 	set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
# 	set (UXSCALE 1)
# else (CMAKE_BUILD_TYPE MATCHES "xscale")
# 	set (STRIP "strip")
# 	set (UXSCALE 0)
# endif (CMAKE_BUILD_TYPE MATCHES "xscale")

add_subdirectory (system)
add_subdirectory (utility)
add_subdirectory (algorithm)
add_subdirectory (clientserver)
add_subdirectory (test)
add_subdirectory (application)

