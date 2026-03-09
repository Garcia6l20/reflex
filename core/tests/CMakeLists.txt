set(CMAKE_INCLUDE_CURRENT_DIR ON)

link_libraries(reflex.core)

doctest_add_test(reflex-core-views-test
  views/cartesian-product.cpp
)
doctest_add_test(reflex-core-meta-test
  meta/is_functional.cpp
  meta/annotation-queries.cpp
)
doctest_add_test(reflex-core-to-tuple-test
  test-to-tuple.cpp
)
doctest_add_test(reflex-core-constant_string-test
  test-constant-string.cpp
)
doctest_add_test(reflex-core-parse-test
  test-parse.cpp
)
doctest_add_test(reflex-core-visit-test
  test-visit.cpp
)
doctest_add_test(reflex-core-match-test
  test-match.cpp
)
