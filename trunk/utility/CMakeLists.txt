include_directories(${SOLIDGROUND_SOURCE_DIR}/utility ${SOLIDGROUND_SOURCE_DIR})
add_library (
	utility src/list.cpp src/stream.cpp src/utility.cpp
	#adding the header files for visual studio projects
	common.hpp istream.hpp ostream.hpp queue.hpp 
	stack.hpp streamptr.hpp iostream.hpp list.hpp
	polycontainer.hpp sharedcontainer.hpp stream.hpp
	workpool.hpp
)

