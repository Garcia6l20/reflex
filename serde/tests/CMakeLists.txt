set(CMAKE_INCLUDE_CURRENT_DIR ON)

link_libraries(reflex.serde)

doctest_add_test(reflex-serde-test
  test-serde.cpp
)

doctest_add_test(reflex-serde-json-value-test
  test-json-value.cpp
)
target_link_libraries(reflex-serde-json-value-test PRIVATE reflex.serde_json)

doctest_add_test(reflex-serde-json-test
  test-serde-json.cpp
)
target_link_libraries(reflex-serde-json-test PRIVATE reflex.serde_json)

doctest_add_test(reflex-serde-bson-test
  test-serde-bson.cpp
)
target_link_libraries(reflex-serde-bson-test PRIVATE reflex.serde_bson)


doctest_add_test(reflex-serde-test-object-visit
  test-object-visit.cpp
)
target_link_libraries(reflex-serde-test-object-visit PRIVATE reflex.poly)
