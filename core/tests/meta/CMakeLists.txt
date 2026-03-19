doctest_add_test(reflex-core-meta-is_functional-test
  is_functional.cpp
)
doctest_add_test(reflex-core-meta-annotation-queries-test
  annotation-queries.cpp
)

reflex_add_cxx_module_library(reflex.core.test.reg
  MODULE_STD
  SOURCES reg-mod.cppm
)

reflex_add_cxx_module_library(reflex.core.test.reg1
  MODULE_STD
  SOURCES reg-mod1.cppm
)
target_link_libraries(reflex.core.test.reg1 PRIVATE reflex.core.test.reg)

reflex_add_cxx_module_library(reflex.core.test.reg2
  MODULE_STD
  SOURCES reg-mod2.cppm
)
target_link_libraries(reflex.core.test.reg2 PRIVATE reflex.core.test.reg)

doctest_add_test(reflex-core-meta-reg-test
  reg.cpp
)
target_link_libraries(reflex-core-meta-reg-test PRIVATE reflex.core.test.reg reflex.core.test.reg1 reflex.core.test.reg2)
