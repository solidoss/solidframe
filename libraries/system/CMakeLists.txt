set(Sources
	src/debug.cpp
	src/device.cpp
	src/socketaddress.cpp
	src/specific.cpp
	src/thread.cpp
	src/cstring.cpp
	src/sharedbackend.cpp
	src/error.cpp
	src/memory.cpp
	src/memorycache.cpp
)

set(Headers
	atomic.hpp
	binary.hpp
	cassert.hpp
	common.hpp
	condition_boost.hpp
	condition.hpp
	condition_stl.hpp
	convertors.hpp
	cstring.hpp
	debug.hpp
	device.hpp
	directory.hpp
	error.hpp
	exception.hpp
	filedevice.hpp
	function.hpp
	memorycache.hpp
	memory.hpp
	mutex_boost.hpp
	mutex.hpp
	mutex_stl.hpp
	mutualstore.hpp
	seekabledevice.hpp
	sharedbackend.hpp
	socketaddress.hpp
	socketdevice.hpp
	socketinfo.hpp
	specific.hpp
	synchronization.hpp
	thread.hpp
	timespec.hpp
	tuple.hpp
)

set(Inlines
	condition.ipp                                                                                                                                                          
	memorycache.ipp                                                                                                                                                        
	mutex.ipp                                                                                                                                                              
	socketaddress.ipp                                                                                                                                                      
	synchronization.ipp                                                                                                                                                    
	timespec.ipp
)

add_library (solid_system 
	${Sources} ${Headers} ${Inlines}
)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/system)
install (TARGETS solid_system DESTINATION lib)
