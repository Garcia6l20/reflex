link_libraries(reflex.testing)

include(ReflexTesting)
add_executable(test-testcase test-testcase.cpp)
reflex_discover_tests(test-testcase)
