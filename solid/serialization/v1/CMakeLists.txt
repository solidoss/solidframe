
set(Sources
    src/binarybasic.cpp
    src/binary.cpp
    src/typeidmap.cpp
)

set(Headers
    binary.hpp
    binarybasic.hpp
    typeidmap.hpp
)

set(Inlines
)

add_library(solid_serialization_v1 STATIC
    ${Sources} ${Headers} ${Inlines}
)
target_include_directories(solid_serialization_v1 PUBLIC
    $<BUILD_INTERFACE:${SolidFrame_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE src
)

target_link_libraries(solid_serialization_v1 solid_utility solid_system)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/serialization/v1)
install (TARGETS solid_serialization_v1 DESTINATION lib EXPORT SolidFrameConfig)

if(NOT ON_CROSS)
    add_subdirectory(test)
endif()
