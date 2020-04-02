if(SOLID_ON_WINDOWS)
    if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
        set(OPENSSL_TARGET "VC-WIN32")
    else()
        set(OPENSSL_TARGET "VC-WIN64A")
    endif()

    ExternalProject_Add(
        build-openssl
        EXCLUDE_FROM_ALL 1
        PREFIX ${EXTERNAL_DIR}/openssl
        URL https://www.openssl.org/source/openssl-1.1.1f.tar.gz
        DOWNLOAD_NO_PROGRESS OFF
        #URL_MD5 "6f4571e7c5a66ccc3323da6c24be8f05"
        #CONFIGURE_COMMAND ${EXTERNAL_DIR}/openssl/src/build-openssl/config --prefix=${EXTERNAL_DIR} --openssldir=ssl_
        CONFIGURE_COMMAND C:/Strawberry/perl/bin/perl Configure ${OPENSSL_TARGET} --prefix=${EXTERNAL_DIR} no-shared no-unit-test no-tests no-ui
        BUILD_COMMAND nmake
        INSTALL_COMMAND nmake install_sw
        BUILD_IN_SOURCE 1
        LOG_UPDATE ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON
    )

    set(OPENSSL_FOUND TRUE)
    set(OPENSSL_LIBRARIES libssl libcrypto)
else()
    ExternalProject_Add(
        build-openssl
        EXCLUDE_FROM_ALL 1
        PREFIX ${EXTERNAL_DIR}/openssl
        URL https://www.openssl.org/source/openssl-1.1.1d.tar.gz
        DOWNLOAD_NO_PROGRESS ON
        #URL_MD5 "6f4571e7c5a66ccc3323da6c24be8f05"
        CONFIGURE_COMMAND ${EXTERNAL_DIR}/openssl/src/build-openssl/config --prefix=${EXTERNAL_DIR} no-shared no-unit-test no-tests no-ui
        BUILD_COMMAND make
        INSTALL_COMMAND make -j4 install_sw
        BUILD_IN_SOURCE 1
        LOG_UPDATE ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON
    )

    set(OPENSSL_FOUND TRUE)
    set(OPENSSL_LIBRARIES ssl crypto)
endif()


