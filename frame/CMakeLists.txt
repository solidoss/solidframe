# the frame libraries:
set(Sources
	src/manager.cpp
	src/object.cpp
	src/service.cpp
	src/schedulerbase.cpp
	src/reactor.cpp
	src/completion.cpp
	src/sharedstore.cpp
	src/event.cpp
)

set(Headers
	schedulerbase.hpp
	common.hpp
	manager.hpp
	object.hpp
	reactorbase.hpp
	reactor.hpp
	scheduler.hpp
	service.hpp
	timer.hpp
	completion.hpp
	sharedstore.hpp
)

set(Inlines
)

add_library (solid_frame
	${Sources} ${Headers} ${Inlines}
)


add_subdirectory(ipc)
add_subdirectory(aio)
add_subdirectory(file)
