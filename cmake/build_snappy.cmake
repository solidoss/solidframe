configure_file(cmake/build_wrapper.sh.in build_wrapper.sh @ONLY)

ExternalProject_Add(
    build_snappy
    PREFIX ${EXTERN_PATH}/snappy
    URL https://github.com/google/snappy/releases/download/1.1.3/snappy-1.1.3.tar.gz
    CONFIGURE_COMMAND sh ${CMAKE_CURRENT_BINARY_DIR}/build_wrapper.sh ${EXTERN_PATH}/snappy/src/build_snappy/configure --prefix=${EXTERN_PATH} 
                                           --disable-shared
                                           --enable-static
                                           --disable-dependency-tracking
                                           --disable-gtest

    BUILD_COMMAND sh ${CMAKE_CURRENT_BINARY_DIR}/build_wrapper.sh make all
    INSTALL_COMMAND sh ${CMAKE_CURRENT_BINARY_DIR}/build_wrapper.sh make install
    BUILD_IN_SOURCE 1
    LOG_UPDATE ON
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
)
