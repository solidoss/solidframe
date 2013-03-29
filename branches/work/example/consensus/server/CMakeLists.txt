set(Sources
	src/serverobject.cpp
)

set(Headers
	serverobject.hpp
)

set(Inlines
)

add_library (example_consensus_serverobject
	${Sources} ${Headers} ${Inlines}
)



