include_directories(${SOLIDGROUND_SOURCE_DIR} ${SOLIDGROUND_SOURCE_DIR}/clientserver)

add_library(clientserver_aio_udp
	src/talker.cpp
)
