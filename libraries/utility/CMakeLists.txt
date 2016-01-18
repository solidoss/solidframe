set(Sources
	src/list.cpp
	src/memory.cpp
	src/stream.cpp
	src/utility.cpp
	src/dynamictype.cpp
	src/memoryfile.cpp
	src/ioformat.cpp
	src/any.cpp
)

set(Headers
	binaryseeker.hpp
	common.hpp
	iostream.hpp
	istream.hpp
	list.hpp
	memory.hpp
	ostream.hpp
	queue.hpp
	stack.hpp
	stream.hpp
	streampointer.hpp
	workpool.hpp
	dynamictype.hpp
	dynamicpointer.hpp
	memoryfile.hpp
	ioformat.hpp
	innerlist.hpp
	any.hpp
)

set(Inlines
	iostream.ipp
	istream.ipp
	ostream.ipp
	stream.ipp	
)

add_library (solid_utility
	${Sources} ${Headers} ${Inlines}
)


add_subdirectory(test)