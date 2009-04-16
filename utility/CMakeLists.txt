include_directories(${SOLIDGROUND_SOURCE_DIR}/utility)

#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

set(Sources
	src/list.cpp
	src/memory.cpp
	src/stream.cpp
	src/utility.cpp
)

set(Headers
	binaryseeker.hpp
	common.hpp
	iostream.hpp
	istream.hpp
	list.hpp
	memory.hpp
	mutualobjectcontainer.hpp
	ostream.hpp
	polycontainer.hpp
	queue.hpp
	stack.hpp
	stream.hpp
	streampointer.hpp
	string.hpp
	workpool.hpp
)

set(Inlines
	src/iostream.ipp
	src/istream.ipp
	src/ostream.ipp
	src/stream.ipp	
)

add_library (utility
	${Sources} ${Headers} ${Inlines}
)

