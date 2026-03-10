set(CMAKE_INCLUDE_CURRENT_DIR ON)

link_libraries(reflex.serde)

doctest_add_test(reflex-serde-test
  test-serde.cpp
)
doctest_add_test(reflex-serde-json-test
  test-serde-json.cpp
)
target_link_libraries(reflex-serde-json-test PRIVATE reflex.serde_json)