add_executable(example_binary_search example_binary_search.cpp)

target_link_libraries (example_binary_search solid_system)

add_executable (example_bit_count example_bit_count.cpp)

target_link_libraries (example_bit_count solid_utility solid_system ${SYS_BASIC_LIBS})

add_executable (example_max_bit_count example_max_bit_count.cpp)

target_link_libraries (example_max_bit_count solid_utility solid_system ${SYS_BASIC_LIBS})
