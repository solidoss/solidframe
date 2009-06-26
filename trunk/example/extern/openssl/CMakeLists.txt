add_executable (sslclient sslclient.cpp)
set_source_files_properties(sslclient.cpp COMPILE_FLAGS -DOSSL_SOURCE_PATH='"${EXTERN_PATH}"')
add_executable (sslserver sslserver.cpp)
set_source_files_properties(sslserver.cpp COMPILE_FLAGS -DOSSL_SOURCE_PATH='"${EXTERN_PATH}"')

target_link_libraries (sslclient ssl crypto utility system pthread rt dl)
target_link_libraries (sslserver ssl crypto utility system pthread rt dl)
