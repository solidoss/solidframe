#include_directories (${SOLIDGROUND_SOURCE_DIR})

add_library(speca speca.cpp)
add_library(specb specb.cpp)

add_executable(tests tests.cpp)


target_link_libraries (tests speca specb system ${SYS_BASIC_LIBS})

