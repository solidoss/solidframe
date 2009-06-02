
#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

set(Sources
	src/logconnectors.cpp
	src/logmanager.cpp
	src/logrecorders.cpp
)

set(Headers
	logclientdata.hpp
	logconnector.hpp
	logconnectors.hpp
	logdata.hpp
	logmanager.hpp
	logrecorder.hpp
	logrecorders.hpp
	logrecord.hpp
	#logwriter.hpp
)

set(Inlines	
)

add_library (audit_log_manager
	${Sources} ${Headers} ${Inlines}
)
