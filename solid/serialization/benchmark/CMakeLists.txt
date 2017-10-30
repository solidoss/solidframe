include(ExternalProject)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(${CMAKE_SOURCE_DIR}/cmake/build_cereal.cmake)

add_executable(benchmark EXCLUDE_FROM_ALL main.cpp boost/record.cpp solid/record.cpp cereal/record.cpp)

add_dependencies(benchmark build_boost build_cereal)

target_link_libraries(benchmark
    solid_serialization
    solid_utility
    solid_system
    ${Boost_SERIALIZATION_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${SYS_BASIC_LIBS}
)
