
function(reflex_discover_tests TARGET)

  cmake_parse_arguments(
    ""
    ""
    "TEST_PREFIX;TEST_SUFFIX;WORKING_DIRECTORY;TEST_LIST;REPORTER;OUTPUT_DIR;OUTPUT_PREFIX;OUTPUT_SUFFIX;DISCOVERY_MODE"
    "TEST_SPEC;EXTRA_ARGS;PROPERTIES;DL_PATHS"
    ${ARGN}
  )


  if(NOT _WORKING_DIRECTORY)
    set(_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  endif()

  ## Generate a unique name based on the extra arguments
  string(SHA1 args_hash "${_TEST_SPEC} ${_EXTRA_ARGS} ${_REPORTER} ${_OUTPUT_DIR} ${_OUTPUT_PREFIX} ${_OUTPUT_SUFFIX}")
  string(SUBSTRING ${args_hash} 0 7 args_hash)

  set(ctest_include_file "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_include-${args_hash}.cmake")
  set(ctest_tests_file "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_tests-${args_hash}.cmake")

  add_custom_command(
    TARGET ${TARGET} POST_BUILD
    BYPRODUCTS "${ctest_tests_file}"
    COMMAND "${CMAKE_COMMAND}"
    -D "TEST_TARGET=${TARGET}"
    -D "TEST_EXECUTABLE=$<TARGET_FILE:${TARGET}>"
    -D "TEST_EXECUTOR=${crosscompiling_emulator}"
    -D "TEST_WORKING_DIR=${_WORKING_DIRECTORY}"
    -D "TEST_SPEC=${_TEST_SPEC}"
    -D "TEST_EXTRA_ARGS=${_EXTRA_ARGS}"
    -D "TEST_PROPERTIES=${_PROPERTIES}"
    -D "TEST_PREFIX=${_TEST_PREFIX}"
    -D "TEST_SUFFIX=${_TEST_SUFFIX}"
    -D "TEST_LIST=${_TEST_LIST}"
    -D "TEST_REPORTER=${_REPORTER}"
    -D "TEST_OUTPUT_DIR=${_OUTPUT_DIR}"
    -D "TEST_OUTPUT_PREFIX=${_OUTPUT_PREFIX}"
    -D "TEST_OUTPUT_SUFFIX=${_OUTPUT_SUFFIX}"
    -D "TEST_DL_PATHS=${_DL_PATHS}"
    -D "CTEST_FILE=${ctest_tests_file}"
    -P "${_REFLEX_DISCOVER_TESTS_SCRIPT}"
    VERBATIM
  )

  file(WRITE "${ctest_include_file}"
    "if(EXISTS \"${ctest_tests_file}\")\n"
    "  include(\"${ctest_tests_file}\")\n"
    "else()\n"
    "  add_test(${TARGET}_NOT_BUILT-${args_hash} ${TARGET}_NOT_BUILT-${args_hash})\n"
    "endif()\n"
  )

  # Add discovered tests as directory TEST_INCLUDE_FILE if possible
  get_property(test_include_file_set DIRECTORY PROPERTY TEST_INCLUDE_FILE SET)
  if(NOT ${test_include_file_set})
    set_property(DIRECTORY
      PROPERTY TEST_INCLUDE_FILE "${ctest_include_file}"
    )
  else()
    message(FATAL_ERROR "Cannot set more than one TEST_INCLUDE_FILE")
  endif()

endfunction()

set(_REFLEX_DISCOVER_TESTS_SCRIPT
  ${CMAKE_CURRENT_LIST_DIR}/ReflexAddTests.cmake
  CACHE INTERNAL "Reflex full path to ReflexAddTests.cmake helper file"
)
