if(SOLID_ON_FREEBSD)
    set(WITH_TOOLSET --with-toolset=clang)
endif()


if(SOLID_ON_WINDOWS)

    if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
        set(BOOST_ADDRESS_MODEL "32")
    else()
        set(BOOST_ADDRESS_MODEL "64")
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "debug")
        set(VARIANT_BUILD "debug")
    else()
        set(VARIANT_BUILD "release")
    endif()

    set(BOOST_VERSION_MAJOR 1)
    set(BOOST_VERSION_MINOR 70)
    set(BOOST_VERSION_PATCH 0)

    ExternalProject_Add(
        build-boost
        EXCLUDE_FROM_ALL 1
        PREFIX ${EXTERNAL_DIR}/boost
        URL "https://dl.bintray.com/boostorg/release/${BOOST_VERSION_MAJOR}.${BOOST_VERSION_MINOR}.${BOOST_VERSION_PATCH}/source/boost_${BOOST_VERSION_MAJOR}_${BOOST_VERSION_MINOR}_${BOOST_VERSION_PATCH}.zip"
        DOWNLOAD_NO_PROGRESS OFF
        #URL_MD5 "6f4571e7c5a66ccc3323da6c24be8f05"
        CONFIGURE_COMMAND bootstrap.bat
        BUILD_COMMAND b2.exe cxxstd=14  address-model=${BOOST_ADDRESS_MODEL} variant=${VARIANT_BUILD} --abbreviate-paths --hash --with-system --with-thread --with-program_options --with-serialization --with-filesystem --prefix=${EXTERNAL_DIR} link=static threading=multi install
        INSTALL_COMMAND ""
        BUILD_IN_SOURCE 1
        LOG_UPDATE ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON
    )

    set(Boost_FOUND TRUE)
    include_directories("${EXTERNAL_DIR}/include/boost-${BOOST_VERSION_MAJOR}_${BOOST_VERSION_MINOR}")
else()
    ExternalProject_Add(
        build-boost
        EXCLUDE_FROM_ALL 1
        PREFIX ${EXTERNAL_DIR}/boost
        URL "https://dl.bintray.com/boostorg/release/1.70.0/source/boost_1_70_0.tar.bz2"
        DOWNLOAD_NO_PROGRESS ON
        #URL_MD5 "6f4571e7c5a66ccc3323da6c24be8f05"
        CONFIGURE_COMMAND ./bootstrap.sh ${WITH_TOOLSET} --with-libraries=system,thread,program_options,serialization,filesystem
        BUILD_COMMAND ./b2 cxxstd=14 --abbreviate-paths --hash --prefix=${EXTERNAL_DIR} --layout=system link=static threading=multi install
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
endif()
