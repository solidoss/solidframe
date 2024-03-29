add_custom_target(versioning
    COMMAND ${CMAKE_COMMAND} -DINFILE="${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp.in"  -DOUTFILE="${CMAKE_CURRENT_BINARY_DIR}/version.cpp.tmp" -DPROJECT_ROOT_DIR="${SolidFrame_SOURCE_DIR}" -P "${SolidFrame_SOURCE_DIR}/cmake/version_configure_file.cmake"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/version.cpp.tmp" "${CMAKE_CURRENT_BINARY_DIR}/version.cpp"
)

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
    spinlock.hpp
    version.hpp
)

set(Inlines
    socketaddress.ipp
    nanotime.ipp
)


if(SOLID_ON_WINDOWS)
    list(APPEND Sources src/crashhandler_windows.cpp)
    list(APPEND Sources src/stacktrace_windows.cpp)
    list(APPEND Headers src/stacktrace_windows.hpp)
elseif(ON_ANDROID)
    list(APPEND Sources src/crashhandler_android.cpp)
else()
    list(APPEND Sources src/crashhandler_unix.cpp)
endif()


add_library (solid_system STATIC
    ${Sources} ${Headers} ${Inlines}
    "${CMAKE_CURRENT_BINARY_DIR}/version.cpp"
)

set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/version.cpp PROPERTIES GENERATED TRUE)

target_include_directories(solid_system PUBLIC
    $<BUILD_INTERFACE:${SolidFrame_SOURCE_DIR}>
    $<BUILD_INTERFACE:${SolidFrame_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}"
)


add_dependencies(solid_system versioning)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configuration_impl.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/configuration_impl.hpp)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version_impl.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/version_impl.hpp)

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/configuration_impl.hpp DESTINATION include/solid/system)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/version_impl.hpp DESTINATION include/solid/system)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/system)
install (TARGETS solid_system DESTINATION lib EXPORT SolidFrameConfig)

if(SOLID_TEST_ALL OR SOLID_TEST_SYSTEM)
    add_subdirectory(test)
endif()
