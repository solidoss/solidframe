set(Sources
	src/consensusrequest.cpp
	src/consensusregistrar.cpp
)

set(Headers
	consensusrequest.hpp
	consensusregistrar.hpp
	consensusrequestid.hpp
)

set(Inlines
)

add_library (solid_consensus_core
	${Sources} ${Headers} ${Inlines}
)

add_subdirectory(server)
