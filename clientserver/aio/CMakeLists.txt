
add_library(clientserver_aio 
	src/aiosocket.cpp
	src/aioobject.cpp
	src/aioselector.cpp
)

add_subdirectory(tcp)
add_subdirectory(udp)
add_subdirectory(openssl)
