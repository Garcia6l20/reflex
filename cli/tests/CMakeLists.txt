reflex_add_cxx_module_library(reflex.testutils
  src/testutils.cpp
)

link_libraries(reflex.cli reflex.testutils)

doctest_add_test(reflex-cli-test-echo
  test-echo.cpp
)

add_executable(fake-git fake-git.cpp)
target_precompile_headers(fake-git REUSE_FROM reflex.std)
target_link_libraries(fake-git PRIVATE reflex.cli)

add_executable(fake-git-shell fake-git-shell.cpp)
target_precompile_headers(fake-git-shell REUSE_FROM reflex.std)
target_link_libraries(fake-git-shell PRIVATE reflex.shell)

doctest_add_test(reflex-cli-test-git
  test-git.cpp
)