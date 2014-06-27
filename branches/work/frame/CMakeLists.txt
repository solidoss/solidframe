# the frame libraries:
set(Sources
	src/manager.cpp
	src/object.cpp
	src/selector.cpp
	src/service.cpp
	src/schedulerbase.cpp
	src/sharedstore.cpp
)

set(Headers
	schedulerbase.hpp
	message.hpp
	common.hpp
	manager.hpp
	object.hpp
	selector.hpp
	scheduler.hpp
	service.hpp
	selectorbase.hpp
	sharedstore.hpp
)

set(Inlines
)

add_library (solid_frame
	${Sources} ${Headers} ${Inlines}
)


#add_subdirectory(ipc)
#add_subdirectory(ipc2)
#add_subdirectory(aio)
#add_subdirectory(file)
