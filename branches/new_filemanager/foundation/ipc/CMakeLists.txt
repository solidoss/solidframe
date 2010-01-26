
set(Sources
	src/ipcservice.cpp
	src/ipctalker.cpp
	src/processconnector.cpp
)

set(Headers
	connectoruid.hpp
	ipcservice.hpp
	src/iodata.hpp
	src/ipctalker.hpp
	src/processconnector.hpp
)

set(Inlines
)

add_library (foundation_ipc
	${Sources} ${Headers} ${Inlines}
)
