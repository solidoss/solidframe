
add_executable (example_dynamic_id example_dynamic_id.cpp)

target_link_libraries (example_dynamic_id solid_utility solid_system ${SYS_BASIC_LIBS})

add_executable (example_dynamic_vs_dynamic_cast example_dynamic_vs_dynamic_cast.cpp)

target_link_libraries (example_dynamic_vs_dynamic_cast solid_utility solid_system ${SYS_BASIC_LIBS})