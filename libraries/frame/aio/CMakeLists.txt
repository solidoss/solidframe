
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
	aiocommon.hpp
	aiocompletion.hpp
	aiodatagram.hpp
	aioerror.hpp
	aioforwardcompletion.hpp
	aiolistener.hpp
	aioobject.hpp
	aioreactorcontext.hpp
	aioreactor.hpp
	aioresolver.hpp
	aiosocket.hpp
	aiostream.hpp
	aiotimer.hpp
)

set(Inlines
)

add_library (solid_frame_aio
	${Sources} ${Headers} ${Inlines}
)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/frame/aio)
install (TARGETS solid_frame_aio DESTINATION lib)

add_subdirectory(openssl)

