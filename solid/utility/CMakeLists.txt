set(Sources
    src/list.cpp
    src/utility.cpp
    src/dynamictype.cpp
    src/memoryfile.cpp
    src/ioformat.cpp
    src/any.cpp
    src/event.cpp
)

set(Headers
    algorithm.hpp
    any.hpp
    common.hpp
    dynamicpointer.hpp
    dynamictype.hpp
    event.hpp
    innerlist.hpp
    ioformat.hpp
    list.hpp
    memoryfile.hpp
    queue.hpp
    sharedmutex.hpp
    stack.hpp
    string.hpp
    workpool.hpp
)

set(Inlines
)

add_library (solid_utility STATIC
    ${Sources} ${Headers} ${Inlines}
)
target_include_directories(solid_utility PUBLIC
    $<BUILD_INTERFACE:${SolidFrame_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE src
)
target_link_libraries(solid_utility solid_system)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/utility)
install (TARGETS solid_utility DESTINATION lib EXPORT SolidFrameConfig)

if(NOT ON_CROSS)
    add_subdirectory(test)
endif()
