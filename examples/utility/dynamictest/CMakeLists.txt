
add_executable (example_dynamictest_ids dynamictest_ids.cpp)

target_link_libraries (example_dynamictest_ids solid_utility solid_system ${SYS_BASIC_LIBS})

add_executable (example_dynamic_vs_dcast dynamic_vs_dcast.cpp)

target_link_libraries (example_dynamic_vs_dcast solid_utility solid_system ${SYS_BASIC_LIBS})