#include <doctest/doctest.h>

#include <meta>
#include <print>
#include <reflex/core.hpp>

TEST_CASE("caseconv: basics")
{
  using namespace reflex;
  using namespace reflex::caseconv;

  REQUIRE(to_snake_case("hello_world") == "hello_world");
  REQUIRE(to_camel_case("helloWorld") == "helloWorld");
  REQUIRE(to_kebab_case("hello-world") == "hello-world");
  REQUIRE(to_pascal_case("HelloWorld") == "HelloWorld");

  REQUIRE(to_snake_case("helloWorld") == "hello_world");
  REQUIRE(to_camel_case("hello_world") == "helloWorld");
  REQUIRE(to_kebab_case("helloWorld") == "hello-world");
  REQUIRE(to_pascal_case("hello_world") == "HelloWorld");

  static constexpr constant_string s1 = to_snake_case("helloWorld");
  static_assert(s1 == "hello_world");
}
