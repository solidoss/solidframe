
set(Sources
	src/binary.cpp
	src/typemapper.cpp
)

set(Headers
	binary.hpp
	idtypemapper.hpp
)

set(Inlines
)

add_library(solid_serialization
	${Sources} ${Headers} ${Inlines}
)
