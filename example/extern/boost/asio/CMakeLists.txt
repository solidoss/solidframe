add_executable(proxyserver proxyserver.cpp)

# if (WIN32 OR APPLE OR LINUX)
# else ()
#     set (SolarisSocketLibs -lsocket -lnsl)
# endif ()

target_link_libraries (proxyserver utility boost_thread boost_system pthread)

