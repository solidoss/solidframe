include_directories(${SOLIDGROUND_SOURCE_DIR})
add_library (
	audit_log_manager src/logconnectors.cpp src/logmanager.cpp src/logrecorders.cpp
)