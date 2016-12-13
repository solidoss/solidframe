set(Sources
	src/filestore.cpp
	src/filestream.cpp
)
set(Headers
	tempbase.hpp
	filestore.hpp
	filestream.hpp
)

set(Inlines
)

add_library (solid_frame_file STATIC
	${Sources} ${Headers} ${Inlines}
)

target_link_libraries(solid_frame_file solid_frame)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/frame/file)
install (TARGETS solid_frame_file DESTINATION lib EXPORT SolidFrameConfig)
