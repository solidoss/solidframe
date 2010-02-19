#include_directories (${SOLIDGROUND_SOURCE_DIR})

add_executable(mutual mutual.cpp)

target_link_libraries (mutual utility system)
