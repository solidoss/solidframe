set(Sources
	src/gammaservice.cpp
	src/gammaconnection.cpp
	src/gammacommands.cpp
	src/gammareader.cpp
	src/gammawriter.cpp
)

set(Headers
	gammaservice.hpp
	src/gammaconnection.hpp
	src/gammacommand.hpp
	src/gammafilters.hpp
	src/gammacommands.hpp
	src/gammareader.hpp
	src/gammawriter.hpp
)

set(Includes
)

add_library(concept_gamma
	${Sources} ${Headers} ${Includes}
)
