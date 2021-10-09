
find_package (Git)

if (GIT_FOUND)
    message("git found: ${GIT_EXECUTABLE} with version ${GIT_VERSION_STRING}")

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" "rev-parse" "HEAD"
        WORKING_DIRECTORY  ${PROJECT_ROOT_DIR}
        OUTPUT_VARIABLE PROJECT_VERSION_VCS_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" "rev-parse" "--abbrev-ref" "HEAD"
        WORKING_DIRECTORY  ${PROJECT_ROOT_DIR}
        OUTPUT_VARIABLE PROJECT_VERSION_VCS_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE)
endif (GIT_FOUND)

string(TIMESTAMP PROJECT_VERSION_TIME "%y%m%d%H%M")

configure_file("${INFILE}" "${OUTFILE}")
