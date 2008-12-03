include_directories(${SOLIDGROUND_SOURCE_DIR}/algorithm)

add_library(serialization 
	src/binary.cpp src/typemapper.cpp
)
