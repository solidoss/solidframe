
set(boost_PREFIX ${EXTERN_PATH})
ExternalProject_Add(
    boost
    PREFIX ${EXTERN_PATH}/boost
    URL "http://sourceforge.net/projects/boost/files/boost/1.63.0/boost_1_63_0.tar.bz2"
    #URL_MD5 "6f4571e7c5a66ccc3323da6c24be8f05"
    CONFIGURE_COMMAND ${EXTERN_PATH}/boost/src/boost/bootstrap.sh --with-libraries=system,thread,program_options --prefix=${EXTERN_PATH}
    BUILD_COMMAND ./bjam link=static cxxflags='-fPIC'
    INSTALL_COMMAND ./bjam link=static install
    BUILD_IN_SOURCE 1
    LOG_UPDATE ON
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
)

set(Boost_FOUND TRUE)
set(Boost_PROGRAM_OPTIONS_LIBRARY boost_program_options)
set(Boost_SYSTEM_LIBRARY boost_system)
