if(SOLID_ON_FREEBSD)
    set(WITH_TOOLSET --with-toolset=clang)
endif()

ExternalProject_Add(
    build-boost
    EXCLUDE_FROM_ALL 1
    PREFIX ${EXTERNAL_DIR}/boost
    URL "https://dl.bintray.com/boostorg/release/1.69.0/source/boost_1_69_0.tar.bz2"
    DOWNLOAD_NO_PROGRESS ON
    #URL_MD5 "6f4571e7c5a66ccc3323da6c24be8f05"
    CONFIGURE_COMMAND ./bootstrap.sh ${WITH_TOOLSET} --with-libraries=system,thread,program_options,serialization,filesystem
    BUILD_COMMAND ./b2 --prefix=${EXTERNAL_DIR} --layout=system link=static threading=multi install
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
