
set(Sources
	src/ipcservice.cpp
	src/ipcconnection.cpp
	src/ipclistener.cpp
)

set(Headers
	ipcsessionuid.hpp
	ipcservice.hpp
	src/ipcconnection.hpp
	src/ipclistener.hpp
	src/ipcutility.hpp
)

set(Inlines
)

add_library (solid_frame_ipc2
	${Sources} ${Headers} ${Inlines}
)
