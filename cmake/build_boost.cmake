if(SOLID_ON_FREEBSD)
    set(WITH_TOOLSET --with-toolset=clang)
endif()

ExternalProject_Add(
    build_boost
    EXCLUDE_FROM_ALL 1
    PREFIX ${EXTERNAL_PATH}/boost
    URL "http://sourceforge.net/projects/boost/files/boost/1.65.1/boost_1_65_1.tar.bz2"
    #URL_MD5 "6f4571e7c5a66ccc3323da6c24be8f05"
    CONFIGURE_COMMAND ./bootstrap.sh ${WITH_TOOLSET} --with-libraries=system,thread,program_options,serialization,filesystem
    BUILD_COMMAND ./b2 --prefix=${EXTERNAL_PATH} --layout=system link=static threading=multi install
    INSTALL_COMMAND ""
    BUILD_IN_SOURCE 1
    LOG_UPDATE ON
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
)

set(Boost_FOUND TRUE)
set(Boost_PROGRAM_OPTIONS_LIBRARY boost_program_options)
set(Boost_SYSTEM_LIBRARY boost_system)
set(Boost_FILESYSTEM_LIBRARY boost_filesystem)
set(Boost_SERIALIZATION_LIBRARY boost_serialization)
