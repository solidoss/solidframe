set(Sources
	src/filestore.cpp
)
set(Headers
	tempbase.hpp
	filestore.hpp
)

set(Inlines
)

add_library (solid_frame_file2
	${Sources} ${Headers} ${Inlines}
)
