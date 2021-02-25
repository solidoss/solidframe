set(Sources
    src/binarybasic.cpp
    src/binaryserializer.cpp
    src/binarydeserializer.cpp
    src/error.cpp
)

set(Headers
    binarybase.hpp
    binarybasic.hpp
    binaryserializer.hpp
    binarydeserializer.hpp
    serialization.hpp
    error.hpp
)

set(Inlines
)

add_library(solid_serialization_v3 STATIC
    ${Sources} ${Headers} ${Inlines}
)
target_include_directories(solid_serialization_v3 PUBLIC
    $<BUILD_INTERFACE:${SolidFrame_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE src
)

target_link_libraries(solid_serialization_v3 solid_utility solid_system solid_reflection_v1)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/serialization/v3)
install (TARGETS solid_serialization_v3 DESTINATION lib EXPORT SolidFrameConfig)

if(NOT SOLID_TEST_NONE OR SOLID_TEST_SERIALIZATION)
    add_subdirectory(test)
endif()

