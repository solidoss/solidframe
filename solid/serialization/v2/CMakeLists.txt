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
    error.hpp
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

target_link_libraries(solid_serialization_v2 solid_utility solid_system)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/serialization/v2)
install (TARGETS solid_serialization_v2 DESTINATION lib EXPORT SolidFrameConfig)

if(NOT ON_CROSS)
    add_subdirectory(test)
endif()
