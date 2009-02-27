#include_directories(${SOLIDGROUND_SOURCE_DIR})

#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

set(Sources
	src/log.cpp
)

set(Headers
	log.hpp
	report.hpp
)

set(Inlines
)

add_library (audit_log
	${Sources} ${Headers} ${Inlines}
)

add_subdirectory (log)
