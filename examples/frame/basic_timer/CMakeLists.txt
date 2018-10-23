add_executable (example_basic_timer example_basic_timer.cpp)

target_link_libraries (example_basic_timer
    solid_frame
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)


