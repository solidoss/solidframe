#include_directories (${SOLIDGROUND_SOURCE_DIR})

link_directories (${SOLIDGROUND_SOURCE_DIR}/system)

add_executable (create create.cpp)
add_executable (open open.cpp)

target_link_libraries (create system boost_filesystem boost_system ${SYS_BASIC_LIBS})
target_link_libraries (open system boost_filesystem boost_system ${SYS_BASIC_LIBS})
