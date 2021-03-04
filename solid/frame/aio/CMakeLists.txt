

set(Sources
    src/aioresolver.cpp
    src/aiocompletion.cpp
    src/aioreactor.cpp
    src/aiolistener.cpp
    src/aioactor.cpp
    src/aioerror.cpp
)

set(Headers
    aiocommon.hpp
    aiocompletion.hpp
    aiodatagram.hpp
    aioerror.hpp
    aioforwardcompletion.hpp
    aiolistener.hpp
    aioactor.hpp
    aioreactorcontext.hpp
    aioreactor.hpp
    aioresolver.hpp
    aiosocket.hpp
	aiosocketbase.hpp
    aiostream.hpp
    aiotimer.hpp
)

set(Inlines
)

add_library (solid_frame_aio STATIC
    ${Sources} ${Headers} ${Inlines}
)

target_include_directories(solid_frame_aio PUBLIC
    $<BUILD_INTERFACE:${SolidFrame_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE src
)


target_link_libraries(solid_frame_aio PUBLIC solid_frame solid_utility solid_system)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/frame/aio)
install (TARGETS solid_frame_aio DESTINATION lib EXPORT SolidFrameConfig)

add_subdirectory(openssl)

if(NOT SOLID_TEST_NONE OR SOLID_TEST_AIO)
    add_subdirectory(test)
endif()
