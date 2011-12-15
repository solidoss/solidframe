
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

add_library(algorithm_serialization_new
	${Sources} ${Headers} ${Inlines}
)
