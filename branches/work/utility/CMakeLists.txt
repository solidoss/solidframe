set(Sources
	src/list.cpp
	src/memory.cpp
	src/stream.cpp
	src/utility.cpp
	src/dynamictype.cpp
	src/memoryfile.cpp
)

set(Headers
	binaryseeker.hpp
	common.hpp
	iostream.hpp
	istream.hpp
	list.hpp
	memory.hpp
	ostream.hpp
	polycontainer.hpp
	queue.hpp
	stack.hpp
	stream.hpp
	streampointer.hpp
	workpool.hpp
	dynamictype.hpp
	dynamicpointer.hpp
	memoryfile.hpp
)

set(Inlines
	iostream.ipp
	istream.ipp
	ostream.ipp
	stream.ipp	
)

add_library (utility
	${Sources} ${Headers} ${Inlines}
)

