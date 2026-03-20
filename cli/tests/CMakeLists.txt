reflex_add_cxx_module_library(reflex.testutils
  testutils.cpp
  testutils.cppm
  pipe_capture.cppm
)

link_libraries(reflex.cli reflex.testutils)

doctest_add_test(reflex-cli-test-echo
  test-echo.cpp
)

reflex_add_cxx_module_library(reflex-cli-test.git
  git.cppm
)

add_executable(fake-git fake-git.cpp)
target_link_libraries(fake-git PRIVATE reflex-cli-test.git)

doctest_add_test(reflex-cli-test-git
  test-git.cpp
)
target_link_libraries(reflex-cli-test-git PRIVATE reflex-cli-test.git)