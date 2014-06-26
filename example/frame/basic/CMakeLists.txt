add_executable (example_basictimer basictimer.cpp)

target_link_libraries (
	solid_frame
	solid_utility
	solid_system
	${SYS_BASIC_LIBS}
)



