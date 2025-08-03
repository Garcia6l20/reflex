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

  CHECK_THAT(json::load<int>(R"(42)") == 42);
  CHECK_THAT(json::load<int>(json::dump(42)) == 42);
}
void test_bool_deserialization()
{
  STATIC_CHECK_THAT(json::load<bool>(R"(true)") == true);
  STATIC_CHECK_THAT(json::load<bool>(R"(false)") == false);
  CHECK_THAT(json::load<bool>(json::dump(true)) == true);
  CHECK_THAT(json::load<bool>(json::dump(false)) == false);
}
void test_string_deserialization()
{
  CHECK_THAT(json::load<std::string>(R"("hello")") == "hello");
  CHECK_THAT(json::load<std::string>(json::dump("hello")) == "hello");
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
  CHECK_THAT(s.i == 42);
  CHECK_THAT(s.d == 42.2);
  // std::println("{}", json::dump(s));
  CHECK_THAT(json::load<S>(json::dump(s)) == s);
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
  CHECK_THAT(s.k == false);
  CHECK_THAT(s.i == 1);
  CHECK_THAT(s.d == 2.2);
  CHECK_THAT(s.s == "ola !");
  CHECK_THAT(s.s1.i == 42);
  CHECK_THAT(s.s1.d == 42.2);
  CHECK_THAT(s.s2.s == "hello");
  CHECK_THAT(s.s2.b == true);
  // std::println("{}", json::dump(s));
  CHECK_THAT(json::load<S>(json::dump(s)) == s);
}
void test_simple_array_deserialization()
{
  auto d = json::load<std::vector<int>>(R"([1,2,3,4])");
  CHECK_THAT(d) == std::vector<int>{1, 2, 3, 4};
  CHECK_THAT(json::load<std::vector<int>>(json::dump(d))) == d;
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
  CHECK_THAT(d) == std::vector<S>{{1, 1.1}, {2, 2.2}, {3, 3.3}, {4, 4.4}};
  CHECK_THAT(json::load<std::vector<S>>(json::dump(d)) == d);
}

void test_unicode()
{
  constexpr auto json_data = "\"\\u2728\"";
  const auto     d         = json::load<std::string>(json_data);
  // std::println("{}", d);
  CHECK_THAT(d == "✨");
  const auto s = json::dump(d);
  std::println("{}", s);
  CHECK_THAT(s != json_data); // NOTE: unicode auto escaping not suppored
  CHECK_THAT(unicode::escape("✨") == "\\u2728");
  CHECK_THAT(unicode::escape(s) == json_data);
  CHECK_THAT(s == unicode::unescape(json_data));
}
} // namespace serialize_tests