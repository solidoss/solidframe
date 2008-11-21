include_directories(${SOLIDGROUND_SOURCE_DIR})
add_library (
	audit_log src/log.cpp
)
add_subdirectory (log)
