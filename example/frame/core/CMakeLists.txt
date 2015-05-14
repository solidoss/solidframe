add_executable (example_basictimer basictimer.cpp)

target_link_libraries (example_basictimer
	solid_frame
	solid_utility
	solid_system
	${SYS_BASIC_LIBS}
)

add_executable (example_basicevent basicevent.cpp)

target_link_libraries (example_basicevent
	solid_frame
	solid_utility
	solid_system
	${SYS_BASIC_LIBS}
)


