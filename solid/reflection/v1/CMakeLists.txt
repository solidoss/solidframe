set(Sources
    src/typemap.cpp
)

set(Headers
    dispatch.hpp
    enummap.hpp
    reflection.hpp
    reflector.hpp
    typemap.hpp
    typetraits.hpp
)
add_library(solid_reflection_v1 STATIC ${Sources} ${Headers})

target_link_libraries(solid_reflection_v1 PUBLIC solid_utility solid_system)

target_include_directories(solid_reflection_v1 PUBLIC
    $<BUILD_INTERFACE:${SolidFrame_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE src
)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/reflection/v1)
install (TARGETS solid_reflection_v1 DESTINATION lib EXPORT SolidFrameConfig)

if(SOLID_TEST_ALL OR SOLID_TEST_REFLECTION)
    add_subdirectory(test)
endif()
