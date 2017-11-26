
set(cereal_PREFIX ${CMAKE_BINARY_DIR}/external/cereal)

ExternalProject_Add(
    build_cereal
    PREFIX ${cereal_PREFIX}
    URL "https://github.com/USCiLab/cereal/archive/v1.2.2.tar.gz"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND mkdir -p ${CMAKE_BINARY_DIR}/external/include/ && cp -r ${cereal_PREFIX}/src/build_cereal/include/cereal ${CMAKE_BINARY_DIR}/external/include/
)
