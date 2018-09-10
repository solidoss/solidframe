include (CheckIncludeFiles)
include (CheckCXXSourceRuns)
include (CheckCXXCompilerFlag)
INCLUDE(TestBigEndian)

# just as an idea
check_include_files(pthread.h SOLID_USE_PTHREAD)

check_include_files(sys/eventfd.h SOLID_USE_EVENTFD)
check_include_files(sys/epoll.h SOLID_USE_EPOLL)
if(SOLID_USE_EPOLL)
    set(SOLID_USE_EPOLLRDHUP TRUE)
endif()
#check_include_files("unordered_map" HAVE_UNORDERED_MAP)

# check if function local static variables are thread safe
file (READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check/safestatic.cpp" source_code)

# message("source code: ${source_code}")
set(CMAKE_REQUIRED_LIBRARIES ${SYS_BASIC_LIBS})

CHECK_CXX_SOURCE_RUNS("${source_code}" SOLID_USE_SAFE_STATIC)

file (READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check/kqueue.cpp" source_code)

CHECK_CXX_SOURCE_RUNS("${source_code}" SOLID_USE_KQUEUE)

file (READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check/wsapoll.cpp" source_code)

CHECK_CXX_SOURCE_COMPILES("${source_code}" SOLID_USE_WSAPOLL)

file (READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check/gnuatomic.cpp" source_code)

CHECK_CXX_SOURCE_RUNS("${source_code}" SOLID_USE_GNU_ATOMIC)


file (READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check/gccbswap.cpp" source_code)

CHECK_CXX_SOURCE_COMPILES("${source_code}" SOLID_USE_GCC_BSWAP)

TEST_BIG_ENDIAN(SOLID_ON_BIG_ENDIAN)
