
#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

set(Sources
	src/signals.cpp
	src/listener.cpp
	src/manager.cpp
	src/service.cpp
	src/tstring.cpp
	src/quicklz.c
)
set_source_files_properties(src/service.cpp COMPILE_FLAGS -DOSSL_SOURCE_PATH='"${EXTERN_PATH}"')
set(Headers
	signals.hpp
	common.hpp
	listener.hpp
	manager.hpp
	service.hpp
	tstring.hpp
	src/quicklz.h
)

set(Inlines
)

add_library(example_concept_core
	${Sources} ${Headers} ${Inlines}
)

