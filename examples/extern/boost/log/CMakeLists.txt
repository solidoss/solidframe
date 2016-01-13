add_executable(boost_logspeed boostlogspeed.cpp)

target_link_libraries (boost_logspeed solid_system solid_utility ${BOOST_THREAD_LIB} boost_log ${BOOST_SYSTEM_LIB} ${SYS_BASIC_LIBS})