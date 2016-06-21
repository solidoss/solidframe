set(Sources
	src/debug.cpp
	src/device.cpp
	src/socketaddress.cpp
	src/cstring.cpp
	src/error.cpp
	src/memory.cpp
	src/system.cpp
)

set(Headers
	cassert.hpp
	common.hpp
	convertors.hpp
	cstring.hpp
	debug.hpp
	device.hpp
	directory.hpp
	error.hpp
	exception.hpp
	filedevice.hpp
	function.hpp
	memory.hpp
	mutualstore.hpp
	seekabledevice.hpp
	socketaddress.hpp
	socketdevice.hpp
	socketinfo.hpp
	timespec.hpp
)

set(Inlines
	socketaddress.ipp
	timespec.ipp
)

add_library (solid_system 
	${Sources} ${Headers} ${Inlines}
)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/system)
install (TARGETS solid_system DESTINATION lib)
