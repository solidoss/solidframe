# the frame libraries:
set(Sources
    src/manager.cpp
    src/object.cpp
    src/service.cpp
    src/schedulerbase.cpp
    src/reactor.cpp
    src/completion.cpp
    src/sharedstore.cpp
)

set(Headers
    common.hpp
    completion.hpp
    forwardcompletion.hpp
    manager.hpp
    objectbase.hpp
    object.hpp
    reactorbase.hpp
    reactorcontext.hpp
    reactor.hpp
    schedulerbase.hpp
    scheduler.hpp
    service.hpp
    sharedstore.hpp
    timer.hpp
    timestore.hpp
)

set(Inlines
    manager.ipp
)

add_library (solid_frame STATIC
    ${Sources} ${Headers} ${Inlines}
)
target_include_directories(solid_frame PUBLIC
    $<BUILD_INTERFACE:${SolidFrame_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE src
)

target_link_libraries(solid_frame solid_utility)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/frame)
install (TARGETS solid_frame DESTINATION lib EXPORT SolidFrameConfig)

add_subdirectory(aio)
add_subdirectory(file)
add_subdirectory(mpipc)
