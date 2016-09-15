
set(Sources
	src/ipcerror.cpp
	src/ipcservice.cpp
	src/ipclistener.cpp
	src/ipcconnection.cpp
	src/ipcconfiguration.cpp
	src/ipcmessagereader.cpp
	src/ipcmessagewriter.cpp
	src/ipcprotocol.cpp
)

set(Headers
	ipcconfiguration.hpp
	ipccontext.hpp
	ipcerror.hpp
	ipcmessage.hpp
	ipcprotocol.hpp
	ipcprotocol_serialization_v1.hpp
	ipcservice.hpp
	ipcsocketstub.hpp
	ipcsocketstub_openssl.hpp
	ipcsocketstub_plain.hpp
)

set(Inlines
)

add_library (solid_frame_ipc
	${Sources} ${Headers} ${Inlines}
)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/frame/ipc)
install (TARGETS solid_frame_ipc DESTINATION lib)

if(NOT ON_CROSS)
	add_subdirectory(test)
endif()
