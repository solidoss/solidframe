include_directories(${SOLIDGROUND_SOURCE_DIR})
add_library(protocol 
	src/reader.cpp src/logger.cpp  src/namematcher.cpp src/writer.cpp
	
	#header files for visual studio projects
	logger.hpp namematcher.hpp parameter.hpp reader.hpp writer.hpp
)
