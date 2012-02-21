set(Sources
	src/debug.cpp
	src/device.cpp
	src/socketaddress.cpp
	src/specific.cpp
	src/thread.cpp
	src/cstring.cpp
)

set(Headers
	cassert.hpp
	condition.hpp
	debug.hpp
	directory.hpp
	mutex.hpp
	socketaddress.hpp
	specific.hpp
	thread.hpp
	common.hpp
	convertors.hpp
	device.hpp
	filedevice.hpp
	seekabledevice.hpp
	socketdevice.hpp
	synchronization.hpp
	timespec.hpp
	cstring.hpp
	src/atomic.hpp
	src/mutexpool.hpp
)
set(Inlines
	condition.ipp
	mutex.ipp
	socketaddress.ipp
	synchronization.ipp
	timespec.ipp
)

add_library (system 
	${Sources} ${Headers} ${Inlines}
)

