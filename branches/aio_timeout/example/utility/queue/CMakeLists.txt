#include_directories (${SOLIDGROUND_SOURCE_DIR})

add_executable(test test.cpp)

target_link_libraries (test system pthread rt)
