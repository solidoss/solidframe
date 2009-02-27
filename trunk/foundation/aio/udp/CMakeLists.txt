
#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

set(Sources
	src/talker.cpp
)

set(Headers
	multitalker.hpp
	talker.hpp
)

set(Inlines
)

add_library (foundation_aio_udp
	${Sources} ${Headers} ${Inlines}
)

