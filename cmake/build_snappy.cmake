ExternalProject_Add(snappy
   URL https://snappy.googlecode.com/files/snappy-${GIT_TAG}.tar.gz

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
                                           --enable-shared
                                           --disable-static
                                           --disable-dependency-tracking
                                           --disable-gtest

  BUILD_COMMAND make all

  INSTALL_DIR ${CMAKE_BINARY_DIR}/install
  INSTALL_COMMAND make install
          COMMAND rm -r <INSTALL_DIR>/share
          COMMAND rm <INSTALL_DIR>/lib/libsnappy.la

  COMMAND ${CMAKE_COMMAND} -E echo FILE "(COPY lib include DESTINATION \"\${CMAKE_INSTALL_PREFIX}\")" > <INSTALL_DIR>/CMakeLists.txt
)
