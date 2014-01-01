

#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

if(${HAS_EPOLL})
	set(selector_source src/aioselector_epoll.cpp)
endif(${HAS_EPOLL})


if(${HAS_KQUEUE})
	set(selector_source src/aioselector_kqueue.cpp)
endif(${HAS_KQUEUE})


set(Sources
	src/aioobject.cpp
	${selector_source}
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

add_library (solid_frame_aio
	${Sources} ${Headers} ${Inlines}
)

add_subdirectory(openssl)
