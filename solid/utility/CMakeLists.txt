set(Sources
    src/utility.cpp
    src/dynamictype.cpp
    src/memoryfile.cpp
    src/ioformat.cpp
    src/any.cpp
    src/event.cpp
    src/workpool.cpp
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
    memoryfile.hpp
    queue.hpp
    sharedmutex.hpp
    stack.hpp
    string.hpp
    workpool.hpp
    workpool_atomic.hpp
    workpool_mutex.hpp
    function.hpp
    functiontraits.hpp
    typetraits.hpp
    delegate.hpp
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

if(NOT SOLID_TEST_NONE OR SOLID_TEST_UTILITY)
    add_subdirectory(test)
endif()
