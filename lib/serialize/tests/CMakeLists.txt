link_libraries(reflex.serialize)

reflex_add_test(reflex-serialize-simple-test test-simple.cpp)
reflex_add_test(reflex-serialize-poly-test test-poly.cpp)
target_link_libraries(reflex-serialize-poly-test PRIVATE reflex.poly)
