include (CheckIncludeFiles)
include (CheckCXXSourceRuns)
include (CheckCXXCompilerFlag)

# just as an idea
check_include_files(pthread.h HAVE_PTHREAD_H)

check_include_files(sys/eventfd.h HAVE_EVENTFD_H)
#check_include_files("unordered_map" HAVE_UNORDERED_MAP)
check_cxx_compiler_flag(-std=c++0x HAVE_CPP11FLAG)

# check if function local static variables are thread safe
file (READ "${CMAKE_CURRENT_SOURCE_DIR}/check/safestatic.cpp" source_code)

# message("source code: ${source_code}")
set(CMAKE_REQUIRED_LIBRARIES ${SYS_BASIC_LIBS})

CHECK_CXX_SOURCE_RUNS("${source_code}" IS_SAFE_FUNCTION_STATIC)

file (READ "${CMAKE_CURRENT_SOURCE_DIR}/check/unorderedmap.cpp" source_code)

set(CMAKE_REQUIRED_FLAGS -std=c++0x)

CHECK_CXX_SOURCE_RUNS("${source_code}" HAVE_CPP11)
