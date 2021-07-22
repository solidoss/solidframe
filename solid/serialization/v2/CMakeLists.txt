set(Sources
    src/binarybasic.cpp
    src/binaryserializer.cpp
    src/binarydeserializer.cpp
    src/error.cpp
    src/serialization.cpp
)

set(Headers
    binarybase.hpp
    binarybasic.hpp
    binaryserializer.hpp
    binarydeserializer.hpp
    typemapbase.hpp
    typemap.hpp
    serialization.hpp
    error.hpp
    typetraits.hpp
)

set(Inlines
)

add_library(solid_serialization_v2 STATIC
    ${Sources} ${Headers} ${Inlines}
)
target_include_directories(solid_serialization_v2 PUBLIC
    $<BUILD_INTERFACE:${SolidFrame_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE src
)

target_link_libraries(solid_serialization_v2 PUBLIC solid_utility solid_system)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/serialization/v2)
install (TARGETS solid_serialization_v2 DESTINATION lib EXPORT SolidFrameConfig)

if(SOLID_TEST_ALL OR SOLID_TEST_SERIALIZATION)
    add_subdirectory(test)
endif()
