add_library(speca speca.cpp)
add_library(specb specb.cpp)

add_executable(threads tests.cpp)
add_executable(exception exception.cpp)
add_executable(condwait_sigterm condwait_sigterm.cpp)

target_link_libraries (threads speca specb system ${SYS_BASIC_LIBS})
target_link_libraries (exception system ${SYS_BASIC_LIBS})
target_link_libraries (condwait_sigterm system ${SYS_BASIC_LIBS})

