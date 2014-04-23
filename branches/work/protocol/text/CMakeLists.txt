set(Sources
	src/logger.cpp
	src/namematcher.cpp
	src/reader.cpp
	src/writer.cpp
	src/buffer.cpp
)

set(Headers
	logger.hpp
	namematcher.hpp
	parameter.hpp
	reader.hpp
	writer.hpp
	buffer.hpp
)

set(Inlines
	parameter.ipp
)

add_library(solid_protocol_text
	${Sources} ${Headers} ${Inlines}
)
