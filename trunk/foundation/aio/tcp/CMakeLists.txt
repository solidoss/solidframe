
#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

set(Sources
	src/connection.cpp
	src/listener.cpp
)

set(Headers
	connection.hpp
	listener.hpp
	multiconnection.hpp
)

set(Inlines
)

add_library (foundation_aio_tcp
	${Sources} ${Headers} ${Inlines}
)
