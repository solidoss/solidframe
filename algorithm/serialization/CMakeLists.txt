include_directories(${SOLIDGROUND_SOURCE_DIR}/algorithm)

#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

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

add_library(serialization 
	${Sources} ${Headers} ${Inlines}
)
