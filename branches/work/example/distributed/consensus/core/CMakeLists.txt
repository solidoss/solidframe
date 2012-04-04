set(Sources
	src/consensusrequests.cpp
)

set(Headers
	consensusrequests.hpp
	consensusmanager.hpp
)

set(Inlines
)

add_library (example_distributed_consensus_core
	${Sources} ${Headers} ${Inlines}
)

