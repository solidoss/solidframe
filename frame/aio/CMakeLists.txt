
if(${HAS_EPOLL})
	set(reactor_source src/aioreactor_epoll.cpp)
endif(${HAS_EPOLL})


if(${HAS_KQUEUE})
	set(reactor_source src/aioreactor_kqueue.cpp)
endif(${HAS_KQUEUE})


set(Sources
#	src/aioobject.cpp
	src/aioresolver.cpp
	src/aiocompletion.cpp
	${reactor_source}
	src/aiolistener.cpp
	src/aioobject.cpp
#	src/aiosocket.cpp
)

set(Headers
	aioobject.hpp
#	aiosecuresocket.hpp
	aiocompletion.hpp
	aioreactor.hpp
	aioresolver.hpp
	aiolistener.hpp
#	src/aiosocket.hpp
)

set(Inlines
#	src/aiosocket.ipp
)

add_library (solid_frame_aio
	${Sources} ${Headers} ${Inlines}
)

add_subdirectory(openssl)

