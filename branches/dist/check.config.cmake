include (CheckIncludeFiles)
include (CheckCXXSourceRuns)

# just as an idea
check_include_files(pthread.h HAVE_PTHREAD_H)

check_include_files(sys/eventfd.h HAVE_EVENTFD_H)

# check if function local static variables are thread safe
file (READ "${CMAKE_CURRENT_SOURCE_DIR}/check/safestatic.cpp" source_code)

# message("source code: ${source_code}")
set(CMAKE_REQUIRED_LIBRARIES ${SYS_BASIC_LIBS})

CHECK_CXX_SOURCE_RUNS("${source_code}" IS_SAFE_FUNCTION_STATIC)

