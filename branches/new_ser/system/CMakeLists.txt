include_directories(${SOLIDGROUND_SOURCE_DIR} ${SOLIDGROUND_SOURCE_DIR}/system)
add_library (system 
	src/thread.cpp src/specific.cpp src/socketaddress.cpp
	src/device.cpp src/debug.cpp
	
	#header files for visual studio projects
	cassert.hpp convertors.hpp device.hpp filedevice.hpp
	seekabledevice.hpp specific.hpp thread.hpp common.hpp
	debug.hpp directory.hpp mutex.hpp socketaddress.hpp
	synchronization.hpp timespec.hpp
	
	src/atomic.hpp src/mutexpool.hpp
)

