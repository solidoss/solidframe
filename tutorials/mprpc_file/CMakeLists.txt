if(APPLE OR WIN32)
else()
    set(STDFS_LIB stdc++fs)
endif()
    
include(CheckCXXSymbolExists)
CHECK_CXX_SYMBOL_EXISTS(std::filesystem::path::preferred_separator filesystem cxx17fs)

if(cxx17fs)
    add_executable (tutorial_mprpc_file_server mprpc_file_server.cpp mprpc_file_messages.hpp)
    
    target_link_libraries (tutorial_mprpc_file_server
        solid_frame_mprpc
        solid_frame_aio
        solid_frame
        solid_serialization_v3
        solid_utility
        solid_system
        ${SYSTEM_BASIC_LIBRARIES}
        ${STDFS_LIB}
    )

    add_executable (tutorial_mprpc_file_client mprpc_file_client.cpp mprpc_file_messages.hpp)

    target_link_libraries (tutorial_mprpc_file_client
        solid_frame_mprpc
        solid_frame_aio
        solid_frame
        solid_serialization_v3
        solid_utility
        solid_system
        ${SYSTEM_BASIC_LIBRARIES}
    )
else()
    message("C++17 filesystem not found - mprpc_file tutorial not available")
endif()
