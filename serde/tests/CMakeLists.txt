if (NOT REFLEX_CXX_MODULES_ENABLED)
  # serde tests require C++ modules, so skip if not enabled
  return()
endif()

set(REFLEX_TEST_GROUP reflex-serde)

link_libraries(reflex::serde)

doctest_add_test(reflex-serde-test
  test-serde.cpp
)

doctest_add_test(reflex-serde-json-value-test
  test-json-value.cpp
)
target_link_libraries(reflex-serde-json-value-test PRIVATE reflex::serde::json)

doctest_add_test(reflex-serde-json-test
  test-serde-json.cpp
)
target_link_libraries(reflex-serde-json-test PRIVATE reflex::serde::json)
target_compile_options(reflex-serde-json-test PRIVATE -fconcepts-diagnostics-depth=5)

doctest_add_test(reflex-serde-bson-test
  test-serde-bson.cpp
)
target_link_libraries(reflex-serde-bson-test PRIVATE reflex::serde::bson)

doctest_add_test(reflex-serde-json-bson-test
  test-serde-json-bson.cpp
)
target_link_libraries(reflex-serde-json-bson-test PRIVATE reflex::serde::json reflex::serde::bson)


doctest_add_test(reflex-serde-test-object-visit
  test-object-visit.cpp
)
target_link_libraries(reflex-serde-test-object-visit PRIVATE reflex::poly)

find_package(LibXml2 REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBXMLPP REQUIRED libxml++-3.0)

reflex_add_cxx_importable_header_module(libxmlpp-header-unit
  HEADERS
    <libxml++/libxml++.h>
  INCLUDE_DIRECTORIES
    ${LIBXMLPP_INCLUDE_DIRS}
  LIBRARIES
    LibXml2::LibXml2
    ${LIBXMLPP_LIBRARIES}
)

doctest_add_test(reflex-serde-xml
  test-xml.cpp
)

target_link_libraries(reflex-serde-xml PRIVATE reflex::serde libxmlpp-header-unit)
