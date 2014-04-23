set(Sources
	src/alphacommands.cpp
	src/alphaconnection.cpp
	src/alphareader.cpp
	src/alphaservice.cpp
	src/alphawriter.cpp
	src/alphamessages.cpp
	src/alphasteward.cpp
)

set(Headers
	alphaservice.hpp
	src/alphacommand.hpp
	src/alphacommands.hpp
	src/alphaconnection.hpp
	src/alphaprotocolfilters.hpp
	src/alphareader.hpp
	src/alphawriter.hpp
	src/alphamessages.hpp
	src/alphasteward.hpp
)

set(Inlines
)

add_library(example_concept_alpha
	${Sources} ${Headers} ${Inlines}
)
