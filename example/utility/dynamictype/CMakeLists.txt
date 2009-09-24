#include_directories (${SOLIDGROUND_SOURCE_DIR})

#link_directories (${SOLIDGROUND_SOURCE_DIR}/system)

add_executable (dynamictest dynamictest.cpp testcommands.cpp)

target_link_libraries (dynamictest utility system boost_filesystem ${SYS_BASIC_LIBS})
