
set( MyTests
    test_nanotime.cpp
    test_exception.cpp
)

create_test_sourcelist( Tests test_system.cpp ${MyTests})

add_executable(test_system ${Tests})

target_link_libraries(test_system solid_system)


add_test(NAME TestNanoTime      COMMAND  test_system test_nanotime)
add_test(NAME TestException     COMMAND  test_system test_exception "some super text")

