include_directories (${SOLIDGROUND_SOURCE_DIR})

link_directories (${SOLIDGROUND_SOURCE_DIR}/system)

add_executable (alphaclienta alphaclienta.cpp writer.cpp)
add_executable (alphafetch alphafetch.cpp writer.cpp)

target_link_libraries (alphaclienta utility system pthread rt)
target_link_libraries (alphafetch utility system pthread rt)