set(Sources
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

add_library(solid_serialization_v4 STATIC
    ${Sources} ${Headers}
)
target_include_directories(solid_serialization_v4 PUBLIC
    $<BUILD_INTERFACE:${SolidFrame_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE src
)

target_link_libraries(solid_serialization_v4 PUBLIC solid_utility solid_system)

install (FILES ${Headers} DESTINATION include/solid/serialization/v4)
install (TARGETS solid_serialization_v4 DESTINATION lib EXPORT SolidFrameConfig)

if(SOLID_TEST_ALL OR SOLID_TEST_SERIALIZATION)
    add_subdirectory(test)
endif()
