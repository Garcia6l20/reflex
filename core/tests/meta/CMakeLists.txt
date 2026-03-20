set(CMAKE_INCLUDE_CURRENT_DIR ON)

doctest_add_test(reflex-core-meta-is_functional-test
  is_functional.cpp
)
doctest_add_test(reflex-core-meta-annotation-queries-test
  annotation-queries.cpp
)

doctest_add_test(reflex-core-meta-reg-test
  reg.cpp
)
target_link_libraries(reflex-core-meta-reg-test PRIVATE reflex.core)
