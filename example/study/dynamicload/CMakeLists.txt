

add_library(study_dltesta dltesta.cpp)

add_library(study_dltestb MODULE dltestb.cpp)
add_library(study_dltestc MODULE dltestc.cpp)

add_executable(study_dltest dltest.cpp)

target_link_libraries(study_dltest study_dltesta solid_system ${SYS_BASIC_LIBS} ${SYS_DYNAMIC_LOAD_LIB})
if(ON_MACOS)
    target_link_libraries(study_dltestb study_dltesta system)
    target_link_libraries(study_dltestc study_dltesta system)
endif(ON_MACOS)
