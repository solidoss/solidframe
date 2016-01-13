set(Sources
	src/compressor.cpp
	src/quicklz.c
)

set(Headers
	compressor.hpp
	src/quicklz.h
)

set(Inlines
)

add_library(example_dchat_core
	${Sources} ${Headers} ${Inlines}
)
