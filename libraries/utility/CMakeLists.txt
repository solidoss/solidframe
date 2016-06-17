set(Sources
	src/list.cpp
	src/memory.cpp
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
	common.hpp
	dynamicpointer.hpp
	dynamictype.hpp
	event.hpp
	innerlist.hpp
	ioformat.hpp
	list.hpp
	memoryfile.hpp
	memory.hpp
	queue.hpp
	sharedmutex.hpp
	specificallocator.hpp
	stack.hpp
	string.hpp
	workpool.hpp
)

set(Inlines
)

add_library (solid_utility
	${Sources} ${Headers} ${Inlines}
)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/utility)
install (TARGETS solid_utility DESTINATION lib)

add_subdirectory(test)