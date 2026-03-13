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

  using json::null;
  SUBCASE("null")
  {
    in.str("null");
    auto value = json::deserializer::load<json::null_t>(in);
    CHECK(value == null);
  }

  SUBCASE("string")
  {
    in.str(JSON("Hello, world!"));
    auto value = json::deserializer::load<std::string>(in);
    CHECK(value == "Hello, world!");
  }

  SUBCASE("number")
  {
    in.str("42");
    auto value = json::deserializer::load<int>(in);
    CHECK(value == 42);
  }

  SUBCASE("boolean")
  {
    in.str("true");
    auto value = json::deserializer::load<bool>(in);
    CHECK(value == true);
    in.str("false");
    value = json::deserializer::load<bool>(in);
    CHECK(value == false);
  }
}

TEST_CASE("serde::json::deserializer: sequence and map")
{
  std::istringstream in;

  SUBCASE("sequence")
  {
    in.str(JSON([1,2,3]));
    auto value = json::deserializer::load(in);
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
    auto value = json::deserializer::load(in);
    CHECK(value.is_object());
    std::println("Parsed object: {}", value.as<json::object>());
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

  in.str(JSON({"intMember":42,"stringMember":"Hello, world!","double-member":3.14}));

  static_assert(serde::object_visitable_c<S>);
  static_assert(serde::object_visitable_c<S&>);

  SUBCASE("direct load")
  {
    auto value = json::deserializer::load<S>(in);
    CHECK_EQ(value.int_member, 42);
    CHECK_EQ(value.string_member, "Hello, world!");
    CHECK_EQ(value.double_member, 3.14);
  }
}
