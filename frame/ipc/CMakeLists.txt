
set(Sources
	src/ipcservice.cpp
)

set(Headers
	ipcsessionuid.hpp
	ipcservice.hpp
)

set(Inlines
)

add_library (solid_frame_ipc
	${Sources} ${Headers} ${Inlines}
)
