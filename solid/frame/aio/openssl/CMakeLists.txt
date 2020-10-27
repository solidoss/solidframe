if(OPENSSL_FOUND)
    set(Sources
        src/aiosecuresocket.cpp
    )

    set(Headers
        aiosecuresocket.hpp
        aiosecurecontext.hpp
    )

    set(Inlines
    )

    add_library (solid_frame_aio_openssl STATIC
        ${Sources} ${Headers} ${Inlines}
    )

    if(NOT ON_CROSS)
        add_dependencies(solid_frame_aio_openssl build-openssl)
    endif()

    target_link_libraries(solid_frame_aio_openssl solid_frame_aio OpenSSL::SSL OpenSSL::Crypto)

    target_include_directories(solid_frame_aio_openssl PUBLIC
        $<BUILD_INTERFACE:${SolidFrame_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
        PRIVATE src
    )


    install (FILES ${Headers} ${Inlines} DESTINATION include/solid/frame/aio/openssl)
    install (TARGETS solid_frame_aio_openssl DESTINATION lib EXPORT SolidFrameConfig)
endif()
