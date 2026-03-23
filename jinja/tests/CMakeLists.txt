link_libraries(reflex.jinja)

add_library(reflex.jinja.types
  types.cpp
)
target_include_directories(reflex.jinja.types PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
link_libraries(reflex.jinja.types)

doctest_add_test(reflex-jinja-parse-test
  parse.cpp
)

doctest_add_test(reflex-jinja-expr-test
  expr.cpp
)
