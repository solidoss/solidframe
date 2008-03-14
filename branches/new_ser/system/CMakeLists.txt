include_directories(${SOLIDGROUND_SOURCE_DIR} ${SOLIDGROUND_SOURCE_DIR}/system)
add_library (system src/thread.cpp src/specific.cpp src/socketaddress.cpp
src/device.cpp src/debug.cpp)

