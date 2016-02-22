
set(Sources
	src/binarybasic.cpp
	src/binary.cpp
	src/typeidmap.cpp
)

set(Headers
	binary.hpp
	binarybasic.hpp
)

set(Inlines
)

add_library(solid_serialization
	${Sources} ${Headers} ${Inlines}
)

add_subdirectory(test)