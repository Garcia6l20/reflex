doctest_add_test(reflex-core-meta-is_functional-test
  is_functional.cpp
)
doctest_add_test(reflex-core-meta-annotation-queries-test
  annotation-queries.cpp
)

reflex_add_cxx_module_library(reflex.core.test.tu
  MODULE_STD
  SOURCES tu-mod.cppm
)
doctest_add_test(reflex-core-meta-tu-test
  tu.cpp
)
target_link_libraries(reflex-core-meta-tu-test PRIVATE reflex.core.test.tu)
