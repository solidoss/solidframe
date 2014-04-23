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

set(Inlines
)

add_library(example_concept_gamma
	${Sources} ${Headers} ${Inlines}
)
