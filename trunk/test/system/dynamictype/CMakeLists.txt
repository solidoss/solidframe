#include_directories (${SOLIDGROUND_SOURCE_DIR})

#link_directories (${SOLIDGROUND_SOURCE_DIR}/system)

add_executable (dynamictest dynamictest.cpp testcommands.cpp)

target_link_libraries (dynamictest system pthread rt boost_filesystem)
