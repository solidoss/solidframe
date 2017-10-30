set(snappy_PREFIX ${CMAKE_BINARY_DIR}/external/snappy)

ExternalProject_Add(
    build_snappy
    EXCLUDE_FROM_ALL 1
    PREFIX ${snappy_PREFIX}
    URL https://github.com/google/snappy/archive/1.1.7.tar.gz
    CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}/external -DCMAKE_INSTALL_LIBDIR=lib
    LOG_UPDATE ON
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
)

set(SNAPPY_LIB ${CMAKE_BINARY_DIR}/external/lib/libsnappy.a)
