set(BOOST_ROOT "${EXTERNAL_PATH}")
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost
    COMPONENTS
        system
        filesystem
        date_time
        program_options
        serialization
)
