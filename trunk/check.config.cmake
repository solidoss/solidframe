include (CheckIncludeFiles)
include (CheckCXXSourceRuns)

# just as an idea
check_include_files(pthread.h HAVE_PTHREAD_H)


# check if function local static variables are thread safe
file (READ "${CMAKE_CURRENT_SOURCE_DIR}/check/safestatic.cpp" source_code)

message("source code: ${source_code}")

CHECK_CXX_SOURCE_RUNS("${source_code}" IS_SAFE_FUNCTION_STATIC)

