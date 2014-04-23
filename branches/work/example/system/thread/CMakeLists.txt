add_library(example_speca speca.cpp)
add_library(example_specb specb.cpp)

add_executable(example_threads tests.cpp)
add_executable(example_exception exception.cpp)
add_executable(example_condwait_sigterm condwait_sigterm.cpp)
add_executable(example_test_wait test_wait.cpp)

target_link_libraries (example_threads example_speca example_specb solid_system ${SYS_BASIC_LIBS})
target_link_libraries (example_exception solid_system ${SYS_BASIC_LIBS})
target_link_libraries (example_condwait_sigterm solid_system ${SYS_BASIC_LIBS})
target_link_libraries (example_test_wait solid_system ${BOOST_SYSTEM_LIB} ${BOOST_THREAD_LIB} ${SYS_BASIC_LIBS})
