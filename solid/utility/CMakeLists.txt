set(Sources
    src/utility.cpp
    src/memoryfile.cpp
    src/ioformat.cpp
    src/event.cpp
    src/super.cpp
    src/sharedbuffer.cpp
)

set(Headers
    algorithm.hpp
    any.hpp
    common.hpp
    event.hpp
    innerlist.hpp
    ioformat.hpp
    memoryfile.hpp
    queue.hpp
    queue_lockfree.hpp
    sharedmutex.hpp
    stack.hpp
    string.hpp
    workpool.hpp
    workpool_lockfree.hpp
    workpool_locking.hpp
    workpool_base.hpp
    function.hpp
    typetraits.hpp
    delegate.hpp
    super.hpp
    sharedbuffer.hpp
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
target_link_libraries(solid_utility PUBLIC solid_system)
target_compile_features(solid_utility PUBLIC cxx_std_17)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/utility)
install (TARGETS solid_utility DESTINATION lib EXPORT SolidFrameConfig)

if(SOLID_TEST_ALL OR SOLID_TEST_UTILITY)
    add_subdirectory(test)
endif()
