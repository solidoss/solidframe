include_directories (${SOLIDGROUND_SOURCE_DIR})


add_executable(printchars printchars.cpp)

target_link_libraries (printchars system pthread rt)

