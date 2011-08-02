set(Sources
	src/consensusobject.cpp
	src/serverobject.cpp
	src/consensussignals.cpp
	src/timerqueue.cpp
)

set(Headers
	consensusobject.hpp
	serverobject.hpp
	src/timerqueue.hpp
)

set(Inlines
)

add_library (example_distributed_server
	${Sources} ${Headers} ${Inlines}
)



