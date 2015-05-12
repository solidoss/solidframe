
set(Sources
	src/ipcservice.cpp
	src/ipclistener.cpp
	src/ipcconnection.cpp
)

set(Headers
	ipccontext.hpp
	ipcservice.hpp
	ipcmessage.hpp
	ipcconfiguration.hpp
	src/ipclistener.hpp
	src/ipcconnection.hpp
	src/ipcutility.hpp
)

set(Inlines
)

add_library (solid_frame_ipc
	${Sources} ${Headers} ${Inlines}
)

