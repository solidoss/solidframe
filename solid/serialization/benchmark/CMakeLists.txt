add_executable(benchmark EXCLUDE_FROM_ALL main.cpp boost/record.cpp solid/record.cpp)

add_dependencies(benchmark build_boost)

target_link_libraries(benchmark
    solid_serialization
    solid_utility
    solid_system
    ${Boost_SERIALIZATION_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${SYS_BASIC_LIBS}
)
