set(Sources
	src/filemanager.cpp
	src/filebase.cpp
	src/filemappers.cpp
)
set(Headers
	filebase.hpp
	filekey.hpp
	filekeys.hpp
	filemanager.hpp
	filemapper.hpp
	filemappers.hpp
)

set(Inlines
)

add_library (foundation_file
	${Sources} ${Headers} ${Inlines}
)
