

#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

set(Sources
	src/aioobject.cpp
	src/aioselector.cpp
	src/aiosocket.cpp
)

set(Headers
	aioobject.hpp
	aiosecuresocket.hpp
	aioselector.hpp
	src/aiosocket.hpp
)

set(Inlines
	src/aiosocket.ipp
)

add_library (foundation_aio
	${Sources} ${Headers} ${Inlines}
)

#add_subdirectory(tcp)
#add_subdirectory(udp)
add_subdirectory(openssl)
