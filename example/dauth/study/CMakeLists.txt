if(LEVELDB_LIB)

add_executable (study_leveldb testleveldb.cpp)
target_link_libraries (study_leveldb ${LEVELDB_LIB} ${SNAPPY_LIB} ${SYS_BASIC_LIBS})

endif(LEVELDB_LIB)