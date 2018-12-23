set(Sources
    src/device.cpp
    src/socketaddress.cpp
    src/cstring.cpp
    src/error.cpp
    src/memory.cpp
    src/system.cpp
    src/log.cpp
)

set(Headers
    cassert.hpp
    common.hpp
    convertors.hpp
    cstring.hpp
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
    flags.hpp
    log.hpp
    statistic.hpp
    crashhandler.hpp
    chunkedstream.hpp
)

set(Inlines
    socketaddress.ipp
    nanotime.ipp
)


if(SOLID_ON_WINDOWS)
    list(APPEND Sources src/crashhandler_windows.cpp)
    list(APPEND Sources src/stacktrace_windows.cpp)
    list(APPEND Headers src/stacktrace_windows.hpp)
else()
    list(APPEND Sources src/crashhandler_unix.cpp)
endif()

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

if(NOT SOLID_TEST_NONE OR SOLID_TEST_SYSTEM)
    add_subdirectory(test)
endif()
