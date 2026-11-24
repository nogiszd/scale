# Invoked by scale_add_command_test (cmake/ScaleTesting.cmake):
#   cmake -DSCALE_COMMAND=<exe@@arg@@...> -DSCALE_EXPECT_EXIT=<n>
#         [-DSCALE_EXPECT_OUTPUT=<regex>] -P expect_command.cmake

if(NOT DEFINED SCALE_COMMAND OR SCALE_COMMAND STREQUAL "")
    message(FATAL_ERROR "SCALE_COMMAND is not set")
endif()
string(REPLACE "@@" ";" command "${SCALE_COMMAND}")

execute_process(
    COMMAND ${command}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)

set(output "${stdout}${stderr}")
message("${output}")
message("exit code: ${result} (expected ${SCALE_EXPECT_EXIT})")

if(NOT result STREQUAL SCALE_EXPECT_EXIT)
    message(FATAL_ERROR "unexpected exit code ${result}, expected ${SCALE_EXPECT_EXIT}")
endif()
if(NOT SCALE_EXPECT_OUTPUT STREQUAL "" AND NOT output MATCHES "${SCALE_EXPECT_OUTPUT}")
    message(FATAL_ERROR "output does not match regex: ${SCALE_EXPECT_OUTPUT}")
endif()
