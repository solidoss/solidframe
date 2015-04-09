
set(Sources
	src/binary.cpp
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
