set(Sources
	src/consensusobject.cpp
	src/consensusmessages.cpp
	src/timerqueue.cpp
)

set(Headers
	consensusobject.hpp
	src/consensusmessage.hpp
	src/consensusmessages.hpp
	src/timerqueue.hpp
)

set(Inlines
)

add_library (solid_consensus_server
	${Sources} ${Headers} ${Inlines}
)

