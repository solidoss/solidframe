include_directories(${SOLIDGROUND_SOURCE_DIR}/system)

#not a good idea of usign automatic file detection - the projects are not automatiocally updated
#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

set(Sources
	src/debug.cpp
	src/device.cpp
	src/socketaddress.cpp
	src/specific.cpp
	src/thread.cpp
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
	src/atomic.hpp
	src/mutexpool.hpp
)
set(Inlines
	src/condition.ipp
	src/mutex.ipp
	src/socketaddress.ipp
	src/synchronization.ipp
	src/timespec.ipp
)

add_library (system 
	${Sources} ${Headers} ${Inlines}
)

