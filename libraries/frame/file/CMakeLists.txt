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

add_library (solid_frame_file
	${Sources} ${Headers} ${Inlines}
)

