
#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

set(Sources
	src/listener.cpp
	src/manager.cpp
	src/service.cpp
	src/tstring.cpp
)

set(Headers
	signals.hpp
	common.hpp
	connection.hpp
	listener.hpp
	manager.hpp
	object.hpp
	service.hpp
	talker.hpp
	tstring.hpp
	visitor.hpp
)

set(Includes
)

add_library(concept_core
	${Sources} ${Headers} ${Includes}
)

