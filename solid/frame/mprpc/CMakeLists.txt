
set(Sources
    src/mprpcerror.cpp
    src/mprpclistener.cpp
    src/mprpcconnection.cpp
    src/mprpcconfiguration.cpp
    src/mprpcmessagereader.cpp
    src/mprpcmessagewriter.cpp
    src/mprpcprotocol.cpp
    src/mprpcrelayengine.cpp
    src/mprpcrelayengines.cpp
    src/mprpcservice.cpp
)

set(Headers
    mprpcconfiguration.hpp
    mprpccontext.hpp
    mprpcerror.hpp
    mprpcmessage.hpp
    mprpcprotocol.hpp
    mprpcprotocol_serialization_v1.hpp
    mprpcprotocol_serialization_v2.hpp
    mprpcservice.hpp
    mprpcsocketstub.hpp
    mprpcsocketstub_openssl.hpp
    mprpcsocketstub_plain.hpp
    mprpccompression_snappy.hpp
    mprpcrelayengine.hpp
    mprpcrelayengines.hpp
    mprpcmessageflags.hpp
)

set(Inlines
)

add_library (solid_frame_mprpc STATIC
    ${Sources} ${Headers} ${Inlines}
)
target_include_directories(solid_frame_mprpc PUBLIC
    $<BUILD_INTERFACE:${SolidFrame_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE src
)

target_link_libraries(solid_frame_mprpc solid_frame_aio)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/frame/mprpc)
install (TARGETS solid_frame_mprpc DESTINATION lib EXPORT SolidFrameConfig)

if(NOT SOLID_TEST_NONE OR SOLID_TEST_MPRPC)
    add_subdirectory(test)
endif()
