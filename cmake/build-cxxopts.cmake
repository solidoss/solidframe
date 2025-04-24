set(cxxopts_PREFIX ${CMAKE_BINARY_DIR}/external/cxxopts)

ExternalProject_Add(
    build-cxxopts
    EXCLUDE_FROM_ALL 1
    PREFIX ${cxxopts_PREFIX}
    #URL https://github.com/jarro2783/cxxopts/archive/refs/tags/v3.0.0.tar.gz
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    DOWNLOAD_NO_PROGRESS ON
    CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}/external -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_BUILD_TYPE=${CONFIGURATION_TYPE}
    BUILD_COMMAND ${CMAKE_COMMAND} --build . --config ${CONFIGURATION_TYPE}
    INSTALL_COMMAND ${CMAKE_COMMAND} --build . --config ${CONFIGURATION_TYPE} --target install
    LOG_UPDATE ON
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
    DOWNLOAD_EXTRACT_TIMESTAMP ON
)
if(SOLID_ON_WINDOWS)
    set(CXXOPTS_LIB ${CMAKE_BINARY_DIR}/external/lib/cxxopts.lib)
else()
    set(CXXOPTS_LIB ${CMAKE_BINARY_DIR}/external/lib/libcxxopts.a)
endif()

