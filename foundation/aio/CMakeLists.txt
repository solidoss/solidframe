

FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

add_library(foundation_aio 
	${Sources} ${Headers}
)

add_subdirectory(tcp)
add_subdirectory(udp)
add_subdirectory(openssl)
