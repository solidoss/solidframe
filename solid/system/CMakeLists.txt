set(Sources
    src/debug.cpp
    src/device.cpp
    src/socketaddress.cpp
    src/cstring.cpp
    src/error.cpp
    src/memory.cpp
    src/system.cpp
)

set(Headers
    cassert.hpp
    common.hpp
    convertors.hpp
    cstring.hpp
    debug.hpp
    device.hpp
    directory.hpp
    error.hpp
    exception.hpp
    filedevice.hpp
    memory.hpp
    mutualstore.hpp
    seekabledevice.hpp
    socketaddress.hpp
    socketdevice.hpp
    socketinfo.hpp
    nanotime.hpp
    pimpl.hpp
)

set(Inlines
    socketaddress.ipp
    nanotime.ipp
)

add_library (solid_system STATIC
    ${Sources} ${Headers} ${Inlines}
)

target_include_directories(solid_system PUBLIC
    $<BUILD_INTERFACE:${SolidFrame_SOURCE_DIR}>
    $<BUILD_INTERFACE:${SolidFrame_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE src
)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/system)
install (TARGETS solid_system DESTINATION lib EXPORT SolidFrameConfig)

if(NOT ON_CROSS)
    add_subdirectory(test)
endif()
