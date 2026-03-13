#include <doctest/doctest.h>

import reflex.serde.json;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::string_view_literals;

#define JSON(...) #__VA_ARGS__

struct[[= serde::naming::camel_case]] S
{
  int                                    int_member;
  std::string                            string_member;
  [[= serde::naming::kebab_case]] double double_member;
};

// static_assert(not json::detail::is_base_value_type(type_of(^^S::int_member)));
// static_assert(json::detail::is_base_value_type(type_of(^^S::string_member)));
// static_assert(json::detail::is_base_value_type(type_of(^^S::double_member)));

TEST_CASE("serde::json::serializer: base types")
{
  std::ostringstream out;
  json::serializer   serializer;

  using json::null;

  SUBCASE("null")
  {
    serializer(out, null);
    CHECK_EQ(out.str(), "null");
  }

  SUBCASE("string")
  {
    serializer(out, "Hello, world!");
    CHECK_EQ(out.str(), "\"Hello, world!\"");
  }

  SUBCASE("number")
  {
    serializer(out, 42);
    CHECK_EQ(out.str(), "42");
  }

  SUBCASE("boolean")
  {
    serializer(out, true);
    CHECK_EQ(out.str(), "true");
    out.str("");
    serializer(out, false);
    CHECK_EQ(out.str(), "false");
  }
}

TEST_CASE("serde::json::serializer: sequence and map")
{
  std::ostringstream out;
  json::serializer   serializer;
  SUBCASE("sequence")
  {
    std::vector<int> arr = {1, 2, 3};
    serializer(out, arr);
    CHECK_EQ(out.str(), JSON([1,2,3]));
  }

  SUBCASE("map")
  {
    auto obj = std::map<std::string, int>{
        {"a", 1},
        {"b", 2}
    };
    serializer(out, obj);
    CHECK_EQ(out.str(), JSON({"a":1,"b":2}));
  }
}

TEST_CASE("serde::json::serializer: aggregate")
{
  std::ostringstream out;
  json::serializer   serializer;

  S s{42, "Hello, world!", 3.14};
  serializer(out, s);
  CHECK_EQ(out.str(), JSON({"intMember":42,"stringMember":"Hello, world!","double-member":3.14}));
}

TEST_CASE("serde::json::serializer: nested aggregate")
{
  std::ostringstream out;
  json::serializer   serializer;

  struct[[= serde::naming::camel_case]] Inner
  {
    int int_member;
  };

  struct[[= serde::naming::camel_case]] Outer
  {
    Inner       inner;
    std::string string_member;
  };

  Outer o{{42}, "Hello, world!"};
  serializer(out, o);
  CHECK_EQ(out.str(), JSON({"inner":{"intMember":42},"stringMember":"Hello, world!"}));
}

TEST_CASE("serde::json::deserializer: base types")
{
  std::istringstream in;
  json::deserializer deserializer;

  using json::null;
  SUBCASE("null")
  {
    in.str("null");
    auto value = deserializer(in);
    CHECK(value == null);
  }

  SUBCASE("string")
  {
    in.str(JSON("Hello, world!"));
    auto value = deserializer(in);
    CHECK(value == "Hello, world!");
  }

  SUBCASE("number")
  {
    in.str("42");
    auto value = deserializer(in);
    CHECK(value == 42);
  }

  SUBCASE("boolean")
  {
    in.str("true");
    auto value = deserializer(in);
    CHECK(value == true);
    in.str("false");
    value = deserializer(in);
    CHECK(value == false);
  }
}

TEST_CASE("serde::json::deserializer: sequence and map")
{
  std::istringstream in;
  json::deserializer deserializer;

  SUBCASE("sequence")
  {
    in.str(JSON([1,2,3]));
    auto value = deserializer(in);
    CHECK(value.is_array());
    CHECK_EQ(value.as<json::array>().size(), 3);
    CHECK_EQ(value.as<json::array>()[0], 1);
    CHECK_EQ(value.as<json::array>()[1], 2);
    CHECK_EQ(value.as<json::array>()[2], 3);
    const auto expected = json::array{1, 2, 3};
    CHECK_EQ(value.as<json::array>(), expected);
  }

  SUBCASE("map")
  {
    in.str(JSON({"a":1,"b":2}));
    auto value = deserializer(in);
    CHECK(value.is_object());
    CHECK_EQ(value.as<json::object>().size(), 2);
    CHECK_EQ(value.as<json::object>().at("a"), 1);
    CHECK_EQ(value.as<json::object>().at("b"), 2);
    const auto expected = json::object{
        {"a", 1},
        {"b", 2}
    };
    CHECK_EQ(value.as<json::object>(), expected);
  }
}

TEST_CASE("serde::json::deserializer: aggregate")
{
  std::istringstream in;
  json::deserializer deserializer;

  in.str(JSON({"intMember":42,"stringMember":"Hello, world!","double-member":3.14}));
  auto value = deserializer.load<S>(in);
  CHECK_EQ(value.int_member, 42);
  CHECK_EQ(value.string_member, "Hello, world!");
  CHECK_EQ(value.double_member, 3.14);
}
