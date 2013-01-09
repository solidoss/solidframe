
set(Sources
	src/ipcbuffer.cpp
	src/ipcservice.cpp
	src/ipctalker.cpp
	src/ipcsession.cpp
	src/ipclistener.cpp
)

set(Headers
	ipcconnectionuid.hpp
	ipcservice.hpp
	src/ipcutility.hpp
	src/ipcbuffer.hpp
	src/ipctalker.hpp
	src/ipcsession.hpp
	src/ipclistener.hpp
)

set(Inlines
)

add_library (foundation_ipc
	${Sources} ${Headers} ${Inlines}
)
