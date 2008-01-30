include_directories(${SOLIDGROUND_SOURCE_DIR} ${SOLIDGROUND_SOURCE_DIR}/clientserver)
add_library(clientserver_core src/execpool.cpp src/object.cpp src/objectselector.cpp
src/server.cpp src/service.cpp src/filekeys.cpp src/filemanager.cpp src/commandexecuter.cpp)
