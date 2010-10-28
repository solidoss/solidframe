set(Sources
	src/signalexecuter.cpp
	src/execpool.cpp
	src/manager.cpp
	src/object.cpp
	src/objectselector.cpp
	src/requestuid.cpp
	src/service.cpp
)

set(Headers
	activeset.hpp
	signalexecuter.hpp
	signal.hpp
	common.hpp
	execpool.hpp
	manager.hpp
	object.hpp
	objectselector.hpp
	objectpointer.hpp
	readwriteobject.hpp
	readwriteservice.hpp
	requestuid.hpp
	selectpool.hpp
	service.hpp
	visitor.hpp
)

set(Inlines
	object.ipp
)

add_library (foundation_core
	${Sources} ${Headers} ${Inlines}
)

add_library (foundation_core_nsm #non singleton manager
	${Sources} ${Headers} ${Inlines}
)

set_target_properties(foundation_core_nsm PROPERTIES COMPILE_FLAGS "-DNSINGLETON_MANAGER")

add_subdirectory(ipc)
add_subdirectory(aio)
add_subdirectory(file)
