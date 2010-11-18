
#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)


set(Sources
	src/betaconnection.cpp
	src/betaservice.cpp
	src/betatalker.cpp
)

set(Headers
	betaservice.hpp
	src/betaconnection.hpp
	src/betatalker.hpp
)

set(Includes
)

add_library(concept_beta
	${Sources} ${Headers} ${Includes}
)

