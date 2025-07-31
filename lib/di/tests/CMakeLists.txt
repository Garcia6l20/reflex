include_directories(${CMAKE_CURRENT_SOURCE_DIR})
link_libraries(reflex.di)

reflex_add_test(reflex-di-simple-test test-simple.cpp)
reflex_add_test(reflex-di6-interfaces-test test-interfaces.cpp)
# reflex_add_test(reflex-di-concept-binding-test test-concept-binding.cpp)
reflex_add_test(reflex-di-concepts-test test-concepts.cpp)
