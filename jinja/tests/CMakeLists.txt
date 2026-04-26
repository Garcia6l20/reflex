if (NOT REFLEX_CXX_MODULES_ENABLED)
  # jinja tests require C++ modules, so skip if not enabled
  return()
endif()

link_libraries(reflex::jinja)

set(REFLEX_TEST_GROUP reflex-jinja)

add_compile_definitions(REFLEX_EXPR_ABORT_ON_NON_MATCHED_ELEMENT)

reflex_add_cxx_module_library(reflex.jinja.types
  types.cpp
  types.cppm
)
target_include_directories(reflex.jinja.types PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

link_libraries(reflex::jinja::types)

doctest_add_test(reflex-jinja-parse-test
  parse.cpp
)

doctest_add_test(reflex-jinja-expr-test
  expr.cpp
)
