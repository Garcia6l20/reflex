reflex_add_cxx_module_library(reflex.testutils
  MODULE_STD
  SOURCES
    pipe_capture.cppm
)

link_libraries(reflex.cli reflex.testutils)

doctest_add_test(reflex-cli-test-echo
  test-echo.cpp
)