include_directories (${SOLIDGROUND_SOURCE_DIR})

add_executable(testbs testbs.cpp)

target_link_libraries (testbs system)
