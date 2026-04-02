set(REFLEX_TEST_GROUP reflex-poly)

link_libraries(reflex.poly)

doctest_add_test(reflex-poly-test-var
  test-var.cpp
)

if (REFLEX_CXX_MODULES_ENABLED)
  doctest_add_test(reflex-poly-test-readme
    test-readme.cpp
  )
endif()