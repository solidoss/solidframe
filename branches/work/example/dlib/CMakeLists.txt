

add_library(example_dltesta dltesta.cpp)

add_library(example_dltestb MODULE dltestb.cpp)
add_library(example_dltestc MODULE dltestc.cpp)

add_executable(example_dltest dltest.cpp)

target_link_libraries(example_dltest example_dltesta system ${SYS_BASIC_LIBS} ${SYS_DYNAMIC_LOAD_LIB})
if(ON_MACOS)
    target_link_libraries(example_dltestb example_dltesta system)
    target_link_libraries(example_dltestc example_dltesta system)
endif(ON_MACOS)
