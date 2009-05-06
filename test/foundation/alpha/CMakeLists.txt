include_directories(${SOLIDGROUND_SOURCE_DIR}/test/foundation)

#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

set(Sources
	src/alphacommands.cpp
	src/alphaconnection.cpp
	src/alphareader.cpp
	src/alphaservice.cpp
	src/alphawriter.cpp
	src/alphasignals.cpp
)
set_source_files_properties(src/alphaservice.cpp COMPILE_FLAGS -DOSSL_SOURCE_PATH='"${EXTERN_PATH}"')
set(Headers
	alphaservice.hpp
	src/alphacommand.hpp
	src/alphacommands.hpp
	src/alphaconnection.hpp
	src/alphaprotocolfilters.hpp
	src/alphareader.hpp
	src/alphawriter.hpp
	src/alphasignals.hpp
)

set(Includes
)

add_library(test_alpha
	${Sources} ${Headers} ${Includes}
)
