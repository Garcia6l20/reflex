#include <reflex/serialize.hpp>

#include <print>

using namespace reflex;
namespace json = reflex::serialize::json;

#include <reflex/testing_main.hpp>

namespace serialize_tests
{
void test_int_deserialization()
{
  // NOTE: from chars is not constexpr

  check_that(json::load<int>(R"(42)") == 42);
  check_that(json::load<int>(json::dump(42)) == 42);
}
void test_bool_deserialization()
{
  static_check_that(json::load<bool>(R"(true)") == true);
  static_check_that(json::load<bool>(R"(false)") == false);
  check_that(json::load<bool>(json::dump(true)) == true);
  check_that(json::load<bool>(json::dump(false)) == false);
}
void test_string_deserialization()
{
  check_that(json::load<std::string>(R"("hello")") == "hello");
  check_that(json::load<std::string>(json::dump("hello")) == "hello");
}
void test_object_deserialization()
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
  check_that(s.i == 42);
  check_that(s.d == 42.2);
  // std::println("{}", json::dump(s));
  check_that(json::load<S>(json::dump(s)) == s);
}
void test_nested_objects_deserialization()
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
  check_that(s.k == false);
  check_that(s.i == 1);
  check_that(s.d == 2.2);
  check_that(s.s == "ola !");
  check_that(s.s1.i == 42);
  check_that(s.s1.d == 42.2);
  check_that(s.s2.s == "hello");
  check_that(s.s2.b == true);
  // std::println("{}", json::dump(s));
  check_that(json::load<S>(json::dump(s)) == s);
}
void test_simple_array_deserialization()
{
  auto d = json::load<std::vector<int>>(R"([1,2,3,4])");
  check_that(d) == std::vector<int>{1, 2, 3, 4};
  check_that(json::load<std::vector<int>>(json::dump(d))) == d;
}
void test_array_of_objects_deserialization()
{
  struct S
  {
    int    i;
    double d;

    constexpr bool operator==(S const&) const = default;
  };
  auto d = json::load<std::vector<S>>(R"([{"i":1, "d":1.1}, {"i":2, "d":2.2}, {"i":3, "d":3.3}, {"i":4, "d":4.4}])");
  check_that(d) == std::vector<S>{{1, 1.1}, {2, 2.2}, {3, 3.3}, {4, 4.4}};
  check_that(json::load<std::vector<S>>(json::dump(d)) == d);
}

void test_unicode()
{
  constexpr auto json_data = "\"\\u2728\"";
  const auto     d         = json::load<std::string>(json_data);
  // std::println("{}", d);
  check_that(d == "✨");
  const auto s = json::dump(d);
  std::println("{}", s);
  check_that(s != json_data); // NOTE: unicode auto escaping not suppored
  check_that(unicode::escape("✨") == "\\u2728");
  check_that(unicode::escape(s) == json_data);
  check_that(s == unicode::unescape(json_data));
}
} // namespace serialize_tests