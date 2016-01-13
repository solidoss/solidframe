add_executable(boost_serialization_polymorphic polymorphic.cpp)

target_link_libraries (boost_serialization_polymorphic ${BOOST_SERIALIZATION_LIB} ${BOOST_SYSTEM_LIB} ${SYS_BASIC_LIBS})