if (NOT REFLEX_CXX_MODULES_ENABLED)
  # serde tests require C++ modules, so skip if not enabled
  return()
endif()

set(REFLEX_TEST_GROUP reflex-serde)

link_libraries(reflex::serde)

reflex_add_cxx_module_library(reflex.serde.tests.types
  types.cppm
)

doctest_add_test(reflex-serde-test
  test-serde.cpp
)

doctest_add_test(reflex-serde-json-value-test
  test-json-value.cpp
)
target_link_libraries(reflex-serde-json-value-test PRIVATE reflex::serde::json)

doctest_add_test(reflex-serde-comments-tree-test
  test-serde-comments-tree.cpp
)
target_link_libraries(reflex-serde-comments-tree-test PRIVATE reflex::serde::json)

doctest_add_test(reflex-serde-json-test
  test-serde-json.cpp
)
target_link_libraries(reflex-serde-json-test PRIVATE reflex::serde::json reflex.serde.tests.types)
target_compile_options(reflex-serde-json-test PRIVATE -fconcepts-diagnostics-depth=5)

doctest_add_test(reflex-serde-json-pretty-test
  test-serde-json-pretty.cpp
)
target_link_libraries(reflex-serde-json-pretty-test PRIVATE reflex::serde::json)

doctest_add_test(reflex-serde-json5-read-test
  test-serde-json5-read.cpp
)
target_link_libraries(reflex-serde-json5-read-test PRIVATE reflex::serde::json)

doctest_add_test(reflex-serde-json5-capture-test
  test-serde-json5-capture.cpp
)
target_link_libraries(reflex-serde-json5-capture-test PRIVATE reflex::serde::json)

doctest_add_test(reflex-serde-json5-write-test
  test-serde-json5-write.cpp
)
target_link_libraries(reflex-serde-json5-write-test PRIVATE reflex::serde::json)

doctest_add_test(reflex-serde-bson-test
  test-serde-bson.cpp
)
target_link_libraries(reflex-serde-bson-test PRIVATE reflex::serde::bson)

doctest_add_test(reflex-serde-xml-test
  test-serde-xml.cpp
)
target_link_libraries(reflex-serde-xml-test PRIVATE reflex::serde::xml reflex.serde.tests.types)

doctest_add_test(reflex-serde-xml-pretty-test
  test-serde-xml-pretty.cpp
)
target_link_libraries(reflex-serde-xml-pretty-test PRIVATE reflex::serde::xml)

doctest_add_test(reflex-serde-xml-capture-test
  test-serde-xml-capture.cpp
)
target_link_libraries(reflex-serde-xml-capture-test PRIVATE reflex::serde::xml)

doctest_add_test(reflex-serde-xml-write-comments-test
  test-serde-xml-write-comments.cpp
)
target_link_libraries(reflex-serde-xml-write-comments-test PRIVATE reflex::serde::xml)

doctest_add_test(reflex-serde-json-bson-test
  test-serde-json-bson.cpp
)
target_link_libraries(reflex-serde-json-bson-test PRIVATE reflex::serde::json reflex::serde::bson)


doctest_add_test(reflex-serde-test-object-visit
  test-object-visit.cpp
)
target_link_libraries(reflex-serde-test-object-visit PRIVATE reflex::poly)
