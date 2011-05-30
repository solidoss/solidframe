set(Sources
	src/signals.cpp
)

set(Headers
	signals.hpp
	manager.hpp
)

set(Inlines
)

add_library (example_distributed_core
	${Sources} ${Headers} ${Inlines}
)

