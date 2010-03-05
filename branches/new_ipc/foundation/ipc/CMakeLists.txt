
set(Sources
	src/ipcservice.cpp
	src/ipctalker.cpp
	src/ipcsession.cpp
)

set(Headers
	ipcconnectionuid.hpp
	ipcservice.hpp
	src/iodata.hpp
	src/ipctalker.hpp
	src/ipcsession.hpp
)

set(Inlines
)

add_library (foundation_ipc
	${Sources} ${Headers} ${Inlines}
)
