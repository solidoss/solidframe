set(Sources
	src/serverobject.cpp
	src/timerqueue.cpp
)

set(Headers
	serverobject.hpp
	src/timerqueue.hpp
)

set(Inlines
)

add_library (example_distributed_server
	${Sources} ${Headers} ${Inlines}
)



