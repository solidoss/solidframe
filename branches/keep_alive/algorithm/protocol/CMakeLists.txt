include_directories(${SOLIDGROUND_SOURCE_DIR})
add_library(protocol 
	src/reader.cpp src/logger.cpp  src/namematcher.cpp src/writer.cpp
)
