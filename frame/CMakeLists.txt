# the frame libraries:
set(Sources
	src/manager.cpp
	src/object.cpp
	src/service.cpp
	src/schedulerbase.cpp
	src/reactor.cpp
	src/timer.cpp
	src/completion.cpp
#	src/sharedstore.cpp
)

set(Headers
	schedulerbase.hpp
	message.hpp
	common.hpp
	manager.hpp
	object.hpp
	reactorbase.hpp
	reactor.hpp
	scheduler.hpp
	service.hpp
	timer.hpp
	completion.hpp
#	sharedstore.hpp
)

set(Inlines
)

add_library (solid_frame
	${Sources} ${Headers} ${Inlines}
)


#add_subdirectory(ipc)
#add_subdirectory(ipc2)
add_subdirectory(aio2)
#add_subdirectory(file)
