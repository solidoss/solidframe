set(Sources
	src/clientobject.cpp
)

set(Headers
	clientobject.hpp
)

set(Inlines
)

add_library (example_consensus_clientobject
	${Sources} ${Headers} ${Inlines}
)


