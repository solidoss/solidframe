add_executable (sslclient sslclient.cpp)
add_executable (sslserver sslserver.cpp)

target_link_libraries (sslclient ssl crypto utility system pthread rt dl)
target_link_libraries (sslserver ssl crypto utility system pthread rt dl)
