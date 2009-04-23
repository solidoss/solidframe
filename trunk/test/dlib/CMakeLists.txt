

add_library(dltesta dltesta.cpp)

add_library(dltestb MODULE dltestb.cpp)
add_library(dltestc MODULE dltestc.cpp)

add_executable(dltest dltest.cpp)

target_link_libraries(dltest dltesta system pthread rt dl)
