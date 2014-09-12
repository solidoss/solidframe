
#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

if(${HAS_EPOLL})
	set(reactor_source src/aioreactor_epoll.cpp)
endif(${HAS_EPOLL})


if(${HAS_KQUEUE})
	set(reactor_source src/aioreactor_kqueue.cpp)
endif(${HAS_KQUEUE})


set(Sources
#	src/aioobject.cpp
	src/aioresolver.cpp
	${reactor_source}
#	src/aiosocket.cpp
)

set(Headers
	aioobject.hpp
#	aiosecuresocket.hpp
	aioreactor.hpp
	aioresolver.hpp
#	src/aiosocket.hpp
)

set(Inlines
#	src/aiosocket.ipp
)

add_library (solid_frame_aio2
	${Sources} ${Headers} ${Inlines}
)

add_subdirectory(openssl)

