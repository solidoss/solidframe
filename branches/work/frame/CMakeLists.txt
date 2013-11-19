# the frame libraries:
set(Sources
	src/messagesteward.cpp
	src/manager.cpp
	src/object.cpp
	src/objectselector.cpp
	src/requestuid.cpp
	src/service.cpp
	src/basicscheduler.cpp
	src/schedulerbase.cpp
)

set(Headers
	schedulerbase.hpp
	messagesteward.hpp
	message.hpp
	common.hpp
	basicscheduler.hpp
	manager.hpp
	object.hpp
	objectselector.hpp
	requestuid.hpp
	scheduler.hpp
	service.hpp
	selectorbase.hpp
)

set(Inlines
	object.ipp
)

add_library (solid_frame_core
	${Sources} ${Headers} ${Inlines}
)


add_subdirectory(ipc)
add_subdirectory(aio)
add_subdirectory(file)
add_subdirectory(temp)