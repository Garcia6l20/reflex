link_libraries(reflex.poly)

link_libraries(reflex.cli)
reflex_add_test(test-var test-var.cpp)
reflex_add_test(test-recursive-var test-recursive-var.cpp)