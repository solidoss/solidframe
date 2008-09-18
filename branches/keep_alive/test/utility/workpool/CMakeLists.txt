include_directories (${SOLIDGROUND_SOURCE_DIR})

add_executable (file_open_pool file_open_pool.cpp)

target_link_libraries (file_open_pool utility system pthread rt boost_filesystem)
