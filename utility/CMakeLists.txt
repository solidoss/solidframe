include_directories(${SOLIDGROUND_SOURCE_DIR}/utility ${SOLIDGROUND_SOURCE_DIR})
add_library (
	utility src/list.cpp src/stream.cpp src/utility.cpp
)

