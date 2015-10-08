
set(Sources
	src/ipcerror.cpp
	src/ipcservice.cpp
	src/ipclistener.cpp
	src/ipcconnection.cpp
	src/ipcconfiguration.cpp
	src/ipcmessagereader.cpp
	src/ipcmessagewriter.cpp
)

set(Headers
	ipccontext.hpp
	ipcservice.hpp
	ipcmessage.hpp
	ipcerror.hpp
	ipcconfiguration.hpp
	ipcserialization.hpp
	src/ipclistener.hpp
	src/ipcconnection.hpp
	src/ipcutility.hpp
	src/ipcmessagereader.hpp
	src/ipcmessagewriter.hpp
)

set(Inlines
)

add_library (solid_frame_ipc
	${Sources} ${Headers} ${Inlines}
)


add_subdirectory(test)