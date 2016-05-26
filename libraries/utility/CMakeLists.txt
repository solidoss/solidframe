set(Sources
	src/list.cpp
	src/memory.cpp
	src/stream.cpp
	src/utility.cpp
	src/dynamictype.cpp
	src/memoryfile.cpp
	src/ioformat.cpp
	src/any.cpp
	src/event.cpp
)

set(Headers
	algorithm.hpp
	any.hpp
	binaryseeker.hpp
	common.hpp
	dynamicpointer.hpp
	dynamictype.hpp
	event.hpp
	innerlist.hpp
	ioformat.hpp
	iostream.hpp
	istream.hpp
	list.hpp
	memoryfile.hpp
	memory.hpp
	ostream.hpp
	queue.hpp
	sharedmutex.hpp
	specificallocator.hpp
	stack.hpp
	stream.hpp
	streampointer.hpp
	string.hpp
	workpool.hpp
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

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/utility)
install (TARGETS solid_utility DESTINATION lib)

add_subdirectory(test)