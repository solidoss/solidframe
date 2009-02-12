include_directories(${SOLIDGROUND_SOURCE_DIR}/test/foundation)

FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)


add_library(test_alpha
	${Sources} ${Headers}
)
