# the frame libraries:
set(Sources
	src/manager.cpp
	src/object.cpp
	src/service.cpp
	src/schedulerbase.cpp
	src/reactor.cpp
	src/completion.cpp
	src/sharedstore.cpp
)

set(Headers
	common.hpp
	completion.hpp
	forwardcompletion.hpp
	manager.hpp
	objectbase.hpp
	object.hpp
	reactorbase.hpp
	reactorcontext.hpp
	reactor.hpp
	schedulerbase.hpp
	scheduler.hpp
	service.hpp
	sharedstore.hpp
	timer.hpp
	timestore.hpp
)

set(Inlines
	manager.ipp
)

add_library (solid_frame
	${Sources} ${Headers} ${Inlines}
)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/frame)
install (TARGETS solid_frame DESTINATION lib)

add_subdirectory(aio)
add_subdirectory(file)

if(NOT ON_ANDROID)
	add_subdirectory(ipc)
endif()
