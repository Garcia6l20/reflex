set(REFLEX_TEST_GROUP reflex-cli)

reflex_add_cxx_module_library(reflex.testutils
  src/testutils.cpp
)

link_libraries(reflex.cli reflex.testutils)

doctest_add_test(reflex-cli-test-echo
  test-echo.cpp
)

add_executable(fake-git fake-git.cpp)
target_link_libraries(fake-git PRIVATE reflex.cli)
add_dependencies(reflex-cli-tests fake-git)

if (REFLEX_CXX_MODULES_ENABLED)
  add_executable(fake-git-shell fake-git-shell.cpp)
  target_link_libraries(fake-git-shell PRIVATE reflex.shell)
  add_dependencies(reflex-cli-tests fake-git-shell)
endif()

doctest_add_test(reflex-cli-test-git
  test-git.cpp
)

doctest_add_test(reflex-cli-test-dynamic-completion
  test-dynamic-completion.cpp
)