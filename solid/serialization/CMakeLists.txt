
set(Sources
	src/binarybasic.cpp
	src/binary.cpp
	src/typeidmap.cpp
)

set(Headers
	binary.hpp
	binarybasic.hpp
	typeidmap.hpp
)

set(Inlines
)

add_library(solid_serialization
	${Sources} ${Headers} ${Inlines}
)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/serialization)
install (TARGETS solid_serialization DESTINATION lib)

if(NOT ON_CROSS)
	add_subdirectory(test)
endif()
