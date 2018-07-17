#set_source_files_properties(boostatomictest.cpp COMPILE_FLAGS -DBOOST_ALL_NO_LIB=1)

add_executable(boost_atomictest boostatomictest.cpp)

target_link_libraries (boost_atomictest ${SYS_BASIC_LIBS})
