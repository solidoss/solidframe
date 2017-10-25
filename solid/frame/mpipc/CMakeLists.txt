
set(Sources
    src/mpipcerror.cpp
    src/mpipcservice.cpp
    src/mpipclistener.cpp
    src/mpipcconnection.cpp
    src/mpipcconfiguration.cpp
    src/mpipcmessagereader.cpp
    src/mpipcmessagewriter.cpp
    src/mpipcprotocol.cpp
    src/mpipcrelay.cpp
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
    mpipcrelay.hpp
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

if(NOT ON_CROSS)
    add_subdirectory(test)
endif()
