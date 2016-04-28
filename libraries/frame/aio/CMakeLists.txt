
if(${SOLID_USE_EPOLL})
	set(reactor_source src/aioreactor_epoll.cpp)
endif(${SOLID_USE_EPOLL})


if(${SOLID_USE_KQUEUE})
	set(reactor_source src/aioreactor_kqueue.cpp)
endif(${SOLID_USE_KQUEUE})


set(Sources
	src/aioresolver.cpp
	src/aiocompletion.cpp
	${reactor_source}
	src/aiolistener.cpp
	src/aioobject.cpp
	src/aioerror.cpp
)

set(Headers
	aioobject.hpp
	aiocompletion.hpp
	aioreactor.hpp
	aioresolver.hpp
	aiolistener.hpp
)

set(Inlines
)

add_library (solid_frame_aio
	${Sources} ${Headers} ${Inlines}
)

add_subdirectory(openssl)

