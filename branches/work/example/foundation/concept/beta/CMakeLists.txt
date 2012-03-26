set(Sources
	src/betaclientcommands.cpp
	src/betaservercommands.cpp
	src/betaconnection.cpp
	src/betaclientconnection.cpp
	src/betaserverconnection.cpp
	src/betasignals.cpp
	src/betaservice.cpp
)

set(Headers
	betaservice.hpp
	betasignals.hpp

	src/betarequests.hpp
	src/betaresponses.hpp
	src/betabuffer.hpp

	src/betaservercommand.hpp
	src/betaservercommands.hpp
	src/betaclientcommand.hpp
	src/betaclientcommands.hpp
	src/betaconnection.hpp
	src/betaclientconnection.hpp
	src/betaserverconnection.hpp
)

set(Inlines
)

add_library(example_concept_beta
	${Sources} ${Headers} ${Inlines}
)
