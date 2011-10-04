add_executable(sockaddrhash sockaddrhash.cpp)

#set_source_files_properties(sockaddrhash.cpp COMPILE_FLAGS -std=c++0x)

target_link_libraries (sockaddrhash system ${SYS_BASIC_LIBS})

