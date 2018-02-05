
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

add_library(solid_serialization STATIC
    ${Sources} ${Headers} ${Inlines}
)
target_include_directories(solid_serialization PUBLIC
    $<BUILD_INTERFACE:${SolidFrame_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE src
)

target_link_libraries(solid_serialization solid_utility solid_system)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/serialization)
install (TARGETS solid_serialization DESTINATION lib EXPORT SolidFrameConfig)

if(NOT ON_CROSS)
    add_subdirectory(test)
endif()

add_subdirectory(v2)
