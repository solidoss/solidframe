
set(Sources
	src/binary.cpp
	src/typemapper.cpp
)

set(Headers
	basetypemap.hpp
	binary.hpp
	idtypemap.hpp
	nametypemap.hpp
	typemapper.hpp
)

set(Inlines
)

add_library(algorithm_serialization 
	${Sources} ${Headers} ${Inlines}
)
