#include_directories(${SOLIDGROUND_SOURCE_DIR}/foundation)

set(Sources
	src/signalexecuter.cpp
	src/execpool.cpp
	src/filekeys.cpp
	src/filemanager.cpp
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
	filekey.hpp
	filekeys.hpp
	filemanager.hpp
	filemapper.hpp
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
	src/object.ipp
)

add_library (foundation_core
	${Sources} ${Headers} ${Inlines}
)

add_subdirectory(ipc)
add_subdirectory(aio)
