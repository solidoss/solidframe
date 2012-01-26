

add_library(dltesta dltesta.cpp)

add_library(dltestb MODULE dltestb.cpp)
add_library(dltestc MODULE dltestc.cpp)

add_executable(dltest dltest.cpp)

target_link_libraries(dltest dltesta system ${SYS_BASIC_LIBS} ${SYS_DYNAMIC_LOAD_LIB})
if(ON_MACOS)
    target_link_libraries(dltestb dltesta system)
    target_link_libraries(dltestc dltesta system)
endif(ON_MACOS)
