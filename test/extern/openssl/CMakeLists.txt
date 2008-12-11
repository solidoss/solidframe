add_executable (sslclient sslclient.cpp)

target_link_libraries (sslclient ssl crypto utility system pthread rt dl)
