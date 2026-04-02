link_libraries(reflex.core)

set(REFLEX_TEST_GROUP "reflex-core")

doctest_add_test(reflex-core-views-test
  views/cartesian-product.cpp
)

add_subdirectory(meta)
add_subdirectory(heapless)

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
if (REFLEX_CXX_MODULES_ENABLED)
  doctest_add_test(reflex-core-caseconv-test
    test-caseconv.cpp
  )
endif()