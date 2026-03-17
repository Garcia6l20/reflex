add_library(reflex.testutils
    testutils.cpp
)
target_sources(reflex.testutils
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
          testutils.cppm
          pipe_capture.cppm
)

link_libraries(reflex.cli reflex.testutils)

doctest_add_test(reflex-cli-test-echo
  test-echo.cpp
)

add_library(reflex-cli-test.git)
target_sources(reflex-cli-test.git
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
          git.cppm
)

add_executable(fake-git fake-git.cpp)
target_link_libraries(fake-git PRIVATE reflex-cli-test.git)

doctest_add_test(reflex-cli-test-git
  test-git.cpp
)
target_link_libraries(reflex-cli-test-git PRIVATE reflex-cli-test.git)