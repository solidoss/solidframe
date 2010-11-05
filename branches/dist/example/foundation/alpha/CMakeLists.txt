set(Sources
	src/alphacommands.cpp
	src/alphaconnection.cpp
	src/alphareader.cpp
	src/alphaservice.cpp
	src/alphawriter.cpp
	src/alphasignals.cpp
)

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

add_library(concept_alpha
	${Sources} ${Headers} ${Includes}
)
