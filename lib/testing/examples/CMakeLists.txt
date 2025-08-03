# NOTE: This example shows many testing failures for demonstration purpose
#       This is why they are not added to cmake test suites
#       In real world project you would likely use reflex_add_test (or reflex_setup_library_test_directory)

link_libraries(reflex.testing)
add_executable(reflex-testing-example-hello-world hello-world.cpp)
add_executable(reflex-testing-example-fixtures fixtures.cpp)
