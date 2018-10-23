add_executable (example_frame_objects example_frame_objects.cpp)

target_link_libraries (example_frame_objects
    solid_frame
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)

