include (CheckIncludeFiles)
include (CheckCXXSourceRuns)
include (CheckCXXCompilerFlag)

# just as an idea
check_include_files(pthread.h HAS_PTHREAD_H)

check_include_files(sys/eventfd.h HAS_EVENTFD)
check_include_files(sys/epoll.h HAS_EPOLL)
#check_include_files("unordered_map" HAVE_UNORDERED_MAP)
check_cxx_compiler_flag(-std=c++0x HAS_CPP11FLAG)

# check if function local static variables are thread safe
file (READ "${CMAKE_CURRENT_SOURCE_DIR}/check/safestatic.cpp" source_code)

# message("source code: ${source_code}")
set(CMAKE_REQUIRED_LIBRARIES ${SYS_BASIC_LIBS})

CHECK_CXX_SOURCE_RUNS("${source_code}" HAS_SAFE_STATIC)

file (READ "${CMAKE_CURRENT_SOURCE_DIR}/check/kqueue.cpp" source_code)

CHECK_CXX_SOURCE_RUNS("${source_code}" HAS_KQUEUE)


file (READ "${CMAKE_CURRENT_SOURCE_DIR}/check/cpp11.cpp" source_code)

set(CMAKE_REQUIRED_FLAGS -std=c++0x)

CHECK_CXX_SOURCE_RUNS("${source_code}" HAS_CPP11)


file (READ "${CMAKE_CURRENT_SOURCE_DIR}/check/gnuatomic.cpp" source_code)

CHECK_CXX_SOURCE_RUNS("${source_code}" HAS_GNU_ATOMIC)

