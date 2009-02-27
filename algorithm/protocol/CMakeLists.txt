
#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

set(Sources
	src/logger.cpp
	src/namematcher.cpp
	src/reader.cpp
	src/writer.cpp
)

set(Headers
	logger.hpp
	namematcher.hpp
	parameter.hpp
	reader.hpp
	writer.hpp
)

set(Inlines
	src/parameter.ipp
)

add_library(protocol 
	${Sources} ${Headers} ${Inlines}
)
