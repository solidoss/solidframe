include_directories(${SOLIDGROUND_SOURCE_DIR} ${SOLIDGROUND_SOURCE_DIR}/clientserver)

add_library(clientserver_aio 
	src/aiosocket.cpp
	src/aioobject.cpp
	src/aioselector.cpp
)

add_subdirectory(tcp)
add_subdirectory(udp)
