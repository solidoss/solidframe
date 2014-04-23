
set(Sources
	src/ipcpacket.cpp
	src/ipcservice.cpp
	src/ipctalker.cpp
	src/ipcsession.cpp
	src/ipclistener.cpp
	src/ipcnode.cpp
)

set(Headers
	ipcconnectionuid.hpp
	ipcservice.hpp
	src/ipcutility.hpp
	src/ipcpacket.hpp
	src/ipctalker.hpp
	src/ipcsession.hpp
	src/ipclistener.hpp
	src/ipcnode.hpp
)

set(Inlines
)

add_library (solid_frame_ipc
	${Sources} ${Headers} ${Inlines}
)
