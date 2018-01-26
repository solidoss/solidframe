
ExternalProject_Add(
    build_openssl
    EXCLUDE_FROM_ALL 1
    PREFIX ${EXTERNAL_PATH}/openssl
    URL https://www.openssl.org/source/openssl-1.1.0g.tar.gz
    #URL_MD5 "6f4571e7c5a66ccc3323da6c24be8f05"
    CONFIGURE_COMMAND ${EXTERNAL_PATH}/openssl/src/build_openssl/config --prefix=${EXTERNAL_PATH} --openssldir=ssl_
    BUILD_COMMAND make
    INSTALL_COMMAND make install_sw
    BUILD_IN_SOURCE 1
    LOG_UPDATE ON
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
)

set(OPENSSL_FOUND TRUE)
set(OPENSSL_LIBRARIES crypto ssl)


