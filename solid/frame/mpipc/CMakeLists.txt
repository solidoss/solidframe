
set(Sources
    src/mpipcerror.cpp
    src/mpipclistener.cpp
    src/mpipcconnection.cpp
    src/mpipcconfiguration.cpp
    src/mpipcmessagereader.cpp
    src/mpipcmessagewriter.cpp
    src/mpipcprotocol.cpp
    src/mpipcrelayengine.cpp
    src/mpipcrelayengines.cpp
    src/mpipcservice.cpp
)

set(Headers
    mpipcconfiguration.hpp
    mpipccontext.hpp
    mpipcerror.hpp
    mpipcmessage.hpp
    mpipcprotocol.hpp
    mpipcprotocol_serialization_v1.hpp
    mpipcservice.hpp
    mpipcsocketstub.hpp
    mpipcsocketstub_openssl.hpp
    mpipcsocketstub_plain.hpp
    mpipccompression_snappy.hpp
    mpipcrelayengine.hpp
    mpipcrelayengines.hpp
)

set(Inlines
)

add_library (solid_frame_mpipc STATIC
    ${Sources} ${Headers} ${Inlines}
)
target_include_directories(solid_frame_mpipc PUBLIC
    $<BUILD_INTERFACE:${SolidFrame_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE src
)

target_link_libraries(solid_frame_mpipc solid_frame_aio)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/frame/mpipc)
install (TARGETS solid_frame_mpipc DESTINATION lib EXPORT SolidFrameConfig)

if(NOT SOLID_TEST_NONE OR SOLID_TEST_MPIPC)
    add_subdirectory(test)
endif()
