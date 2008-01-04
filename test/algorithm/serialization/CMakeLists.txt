include_directories (${SOLIDGROUND_SOURCE_DIR})

link_directories (${SOLIDGROUND_SOURCE_DIR}/system)

add_executable (simple simple.cpp)
add_executable (fork fork.cpp)

target_link_libraries (simple serialization utility system pthread rt)
target_link_libraries (fork serialization utility system pthread rt)
