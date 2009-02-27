#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

set(Sources
	src/proxymulticonnection.cpp
	src/proxyservice.cpp
)

set(Headers
	proxyservice.hpp
	src/proxymulticonnection.hpp
)

set(Includes
)

add_library(test_proxy
	${Sources} ${Headers} ${Includes}
)

