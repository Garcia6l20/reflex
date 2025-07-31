link_libraries(reflex.serialize)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

reflex_add_test(reflex-serialize-simple-test test-simple.cpp)
reflex_add_test(reflex-serialize-poly-test test-poly.cpp)
target_link_libraries(reflex-serialize-poly-test PRIVATE reflex.poly)
