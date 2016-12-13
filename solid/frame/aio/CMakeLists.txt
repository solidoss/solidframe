

set(Sources
	src/aioresolver.cpp
	src/aiocompletion.cpp
	src/aioreactor.cpp
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

add_library (solid_frame_aio STATIC
	${Sources} ${Headers} ${Inlines}
)

target_link_libraries(solid_frame_aio solid_frame)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/frame/aio)
install (TARGETS solid_frame_aio DESTINATION lib EXPORT SolidFrameConfig)

add_subdirectory(openssl)

