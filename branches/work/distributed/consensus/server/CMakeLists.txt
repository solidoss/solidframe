set(Sources
	src/consensusobject.cpp
	src/consensussignals.cpp
	src/timerqueue.cpp
)

set(Headers
	consensusobject.hpp
	src/consensussignal.hpp
	src/consensussignals.hpp
	src/timerqueue.hpp
)

set(Inlines
)

add_library (distributed_consensus_server
	${Sources} ${Headers} ${Inlines}
)

