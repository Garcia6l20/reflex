link_libraries(reflex.jinja)

doctest_add_test(reflex-jinja-parse-test
  parse.cpp
)

doctest_add_test(reflex-jinja-expr-test
  expr.cpp
)
