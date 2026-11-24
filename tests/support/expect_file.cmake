# cmake -DSCALE_FILE=<path> -DSCALE_EXPECT_CONTENT=<regex> -P expect_file.cmake
# Fails unless the file exists and its content matches the regex.

if(NOT EXISTS "${SCALE_FILE}")
    message(FATAL_ERROR "file does not exist: ${SCALE_FILE}")
endif()
file(READ "${SCALE_FILE}" content)
message("${content}")
if(NOT content MATCHES "${SCALE_EXPECT_CONTENT}")
    message(FATAL_ERROR "file content does not match regex: ${SCALE_EXPECT_CONTENT}")
endif()
