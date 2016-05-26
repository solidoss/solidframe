

#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

if(${SOLID_USE_EPOLL})
	set(selector_source src/aioselector_epoll.cpp)
endif(${SOLID_USE_EPOLL})


if(${SOLID_USE_KQUEUE})
	set(selector_source src/aioselector_kqueue.cpp)
endif(${SOLID_USE_KQUEUE})


set(Sources
	src/aioobject.cpp
	src/aioresolver.cpp
	${selector_source}
	src/aiosocket.cpp
)

set(Headers
	aioobject.hpp
	aiosecuresocket.hpp
	aioselector.hpp
	aioresolver.hpp
	src/aiosocket.hpp
)

set(Inlines
	src/aiosocket.ipp
)

add_library (solid_frame_aio
	${Sources} ${Headers} ${Inlines}
)

add_subdirectory(openssl)
