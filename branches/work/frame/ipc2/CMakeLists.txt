
set(Sources
	src/ipcservice.cpp
	src/ipcconnection.cpp
	src/ipclistener.cpp
)

set(Headers
	ipcconnectionuid.hpp
	ipcservice.hpp
	src/ipcconnection.hpp
	src/ipclistener.hpp
)

set(Inlines
)

add_library (solid_frame_ipc2
	${Sources} ${Headers} ${Inlines}
)
