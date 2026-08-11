set(Headers
    serialization.hpp
    deserialization.hpp
)

install (FILES ${Headers} DESTINATION include/solid/serialization/json/v1)

if(SOLID_TEST_ALL OR SOLID_TEST_SERIALIZATION)
    add_subdirectory(test)
endif()
