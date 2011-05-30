set(Sources
	src/serverobject.cpp
)

set(Headers
	serverobject.hpp
)

set(Inlines
)

add_library (example_distributed_server
	${Sources} ${Headers} ${Inlines}
)



