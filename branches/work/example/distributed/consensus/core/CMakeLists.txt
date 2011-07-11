set(Sources
	src/consensusrequests.cpp
)

set(Headers
	consensusrequestid.hpp
	consensusrequest.hpp
	consensusrequests.hpp
	consensusmanager.hpp
)

set(Inlines
)

add_library (example_distributed_core
	${Sources} ${Headers} ${Inlines}
)

