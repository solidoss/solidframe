
# additional target to perform clang-format run, requires clang-format



# get all project files
file(GLOB_RECURSE ALL_SOURCE_FILES solid/*.cpp solid/*.hpp solid/*.ipp examples/*.cpp examples/*.hpp examples/*.ipp tutorials/*.cpp tutorials/*.hpp tutorials/*.ipp)
foreach (SOURCE_FILE ${ALL_SOURCE_FILES})
    #message("Source file: ${SOURCE_FILE}")
    string(FIND ${SOURCE_FILE} ${CMAKE_CURRENT_BINARY_DIR} PROJECT_TRDPARTY_DIR_FOUND)
    if (NOT ${PROJECT_TRDPARTY_DIR_FOUND} EQUAL -1)
        list(REMOVE_ITEM ALL_SOURCE_FILES ${SOURCE_FILE})
    else ()
    endif ()
endforeach ()

add_custom_target(
    clangformat
    COMMAND /usr/bin/clang-format
    -style=file
    -i
    ${ALL_SOURCE_FILES}
)
