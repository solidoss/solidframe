
#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

set(Sources
	src/echoconnection.cpp
	src/echoservice.cpp
	src/echotalker.cpp
)

set(Headers
	echoservice.hpp
	src/echoconnection.hpp
	src/echotalker.hpp
)

set(Includes
)

add_library(test_echo
	${Sources} ${Headers} ${Includes}
)

