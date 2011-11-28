set(Sources
	src/consensusrequest.cpp
)

set(Headers
	consensusrequest.hpp
	consensusrequestid.hpp
)

set(Inlines
)

add_library (distributed_consensus_core
	${Sources} ${Headers} ${Inlines}
)

add_subdirectory(server)
