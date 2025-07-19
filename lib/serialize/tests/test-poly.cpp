#include <reflex/serialize.hpp>

#include <catch2/catch_test_macros.hpp>

#include <print>

using namespace reflex;
namespace json = reflex::serialize::json;

TEST_CASE("deserialization")
{
  SECTION("int deserialization")
  {
    auto v = json::load("42");
    REQUIRE(v.has_value<json::number>());
    REQUIRE(v == json::number{42});
  }
  SECTION("string deserialization")
  {
    auto v = json::load("\"42\"");
    REQUIRE(v.has_value<json::string>());
    REQUIRE(v == json::string{"42"});
  }
  SECTION("bool deserialization")
  {
    REQUIRE(json::load("true") == true);
    REQUIRE(json::load("false") == false);
  }
  SECTION("vec deserialization")
  {
    auto v = json::load("[1,2,3]");
    REQUIRE(v.has_value<json::array>());
    REQUIRE(v[0] == 1.0);
    REQUIRE(v[1] == 2.0);
    REQUIRE(v[2] == 3.0);
  }
  SECTION("obj deserialization")
  {
    auto v = json::load(R"({"a": 1, "b": 2, "c": 3})");
    REQUIRE(v.has_value<json::object>());
    REQUIRE(v["a"] == 1.0);
    REQUIRE(v["b"] == 2.0);
    REQUIRE(v["c"] == 3.0);
  }
}
