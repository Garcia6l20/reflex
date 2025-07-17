#include <reflex/serialize.hpp>

#include <catch2/catch_test_macros.hpp>

#include <print>

using namespace reflex;
namespace json = reflex::serialize::json;

TEST_CASE("deserialization")
{
  SECTION("int deserialization")
  {
    // NOTE: from chars is not constexpr

    REQUIRE(json::load<int>(R"(42)") == 42);
    REQUIRE(json::load<int>(json::dump(42)) == 42);
  }
  SECTION("bool deserialization")
  {
    STATIC_REQUIRE(json::load<bool>(R"(true)") == true);
    STATIC_REQUIRE(json::load<bool>(R"(false)") == false);
    REQUIRE(json::load<bool>(json::dump(true)) == true);
    REQUIRE(json::load<bool>(json::dump(false)) == false);
  }
  SECTION("string deserialization")
  {
    REQUIRE(json::load<std::string>(R"("hello")") == "hello");
    REQUIRE(json::load<std::string>(json::dump("hello")) == "hello");
  }
  SECTION("object deserialization")
  {
    struct S
    {
      int    i;
      double d;

      constexpr bool operator==(S const&) const = default;
    };
    auto s = json::load<S>(R"({
      "i": 42,
      "d": 42.2
    })");
    REQUIRE(s.i == 42);
    REQUIRE(s.d == 42.2);
    // std::println("{}", json::dump(s));
    REQUIRE(json::load<S>(json::dump(s)) == s);
  }
  SECTION("nested objects deserialization")
  {
    struct S
    {
      bool        k = true;
      std::string s;

      struct S1
      {
        int    i;
        double d;

        constexpr bool operator==(S1 const&) const = default;
      } s1;

      struct S2
      {
        std::string s;
        bool        b;

        constexpr bool operator==(S2 const&) const = default;
      } s2;

      int    i;
      double d;

      constexpr bool operator==(S const&) const = default;
    };
    auto s = json::load<S>(R"({
      "k": false,
      "i": 1,
      "d": 2.2
      "s": "ola !",
      "s1": {
        "i": 42,
        "d": 42.2
      },
      "s2": {
        "s": "hello",
        "b": true
      }
    })");
    REQUIRE(s.k == false);
    REQUIRE(s.i == 1);
    REQUIRE(s.d == 2.2);
    REQUIRE(s.s == "ola !");
    REQUIRE(s.s1.i == 42);
    REQUIRE(s.s1.d == 42.2);
    REQUIRE(s.s2.s == "hello");
    REQUIRE(s.s2.b == true);
    // std::println("{}", json::dump(s));
    REQUIRE(json::load<S>(json::dump(s)) == s);
  }
  SECTION("simple array deserialization")
  {
    auto d = json::load<std::vector<int>>(R"([1,2,3,4])");
    REQUIRE(d == std::vector<int>{1, 2, 3, 4});
    REQUIRE(json::load<std::vector<int>>(json::dump(d)) == d);
  }
  SECTION("array of objects deserialization")
  {
    struct S
    {
      int    i;
      double d;

      constexpr bool operator==(S const&) const = default;
    };
    auto d = json::load<std::vector<S>>(R"([{"i":1, "d":1.1}, {"i":2, "d":2.2}, {"i":3, "d":3.3}, {"i":4, "d":4.4}])");
    REQUIRE(d == std::vector<S>{{1, 1.1}, {2, 2.2}, {3, 3.3}, {4, 4.4}});
    REQUIRE(json::load<std::vector<S>>(json::dump(d)) == d);
  }
}

TEST_CASE("unicode test")
{
  SECTION("loading")
  {
    constexpr auto json_data = "\"\\u2728\"";
    const auto     d         = json::load<std::string>(json_data);
    // std::println("{}", d);
    REQUIRE(d == "✨");
    const auto s = json::dump(d);
    std::println("{}", s);
    REQUIRE(s != json_data); // NOTE: unicode auto escaping not suppored
    REQUIRE(unicode::escape("✨") == "\\u2728");
    REQUIRE(unicode::escape(s) == json_data);
    REQUIRE(s == unicode::unescape(json_data));
  }
}
