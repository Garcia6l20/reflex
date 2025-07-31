include_directories(${CMAKE_CURRENT_SOURCE_DIR})
link_libraries(reflex.utility)

reflex_add_test(reflex-utility-tuple-test test-tuple.cpp)

reflex_add_test(reflex-utility-named-tuple-test test-named-tuple.cpp)
reflex_add_test(reflex-utility-error-code-test test-error-code.cpp)
reflex_add_test(reflex-utility-annotation-queries-test test-annotation-queries.cpp)
