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

enum class Color
{
  Red,
  Green,
  Blue
};

enum class[[= reflex::enum_flags]] FilePermissions
{
  None    = 0,
  Read    = 1 << 0,
  Write   = 1 << 1,
  Execute = 1 << 2
};

TEST_CASE("reflex::serde::json::serializer: base types")
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

  SUBCASE("enum")
  {
    serializer(out, Color::Green);
    CHECK_EQ(out.str(), "\"Green\"");
  }

  SUBCASE("enum-flags")
  {
    constexpr auto perms = FilePermissions::Read | FilePermissions::Write;
    serializer(out, perms);
    CHECK_EQ(out.str(), "\"Read|Write\"");
  }
}

TEST_CASE("reflex::serde::json::serializer: sequence and map")
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

TEST_CASE("reflex::serde::json::serializer: aggregate")
{
  std::ostringstream out;
  json::serializer   serializer;

  S s{42, "Hello, world!", 3.14};
  serializer(out, s);
  CHECK_EQ(out.str(), JSON({"intMember":42,"stringMember":"Hello,
  world!","double-member":3.14}));
}

TEST_CASE("reflex::serde::json::serializer: nested aggregate")
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

TEST_CASE("reflex::serde::json::deserializer: base types")
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

  SUBCASE("enum")
  {
    in.str(JSON("Green"));
    auto value = json::deserializer::load<Color>(in);
    CHECK(value == Color::Green);
  }

  SUBCASE("enum-flags")
  {
    in.str(JSON("Read|Write"));
    auto value = json::deserializer::load<FilePermissions>(in);
    CHECK((value & FilePermissions::Read) == FilePermissions::Read);
    CHECK((value & FilePermissions::Write) == FilePermissions::Write);
    CHECK((value & FilePermissions::Execute) == FilePermissions::None);
  }
}

TEST_CASE("reflex::serde::json::deserializer: sequence and map")
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

TEST_CASE("reflex::serde::json::deserializer: aggregate")
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

using custom_var1 = poly::var<json::string, json::number, json::boolean, S>;

TEST_CASE("reflex::serde::json::deserializer: custom var")
{
  std::ostringstream out;
  json::serializer   serializer;

  // in.str(JSON({"test": {"intMember":42,"stringMember":"Hello, world!","double-member":3.14}}));

  SUBCASE("custom var")
  {
    serializer(out, custom_var1{
        {"test", S{42, "Hello, world!", 3.14}}
    });
    std::println("Serialized: {}", out.str());
    auto value = json::deserializer::load<custom_var1>(out.str());
    std::println("Deserialized: {}", value);
    CHECK_EQ(value["test"].as<S>().int_member, 42);
    CHECK_EQ(value["test"].as<S>().string_member, "Hello, world!");
    CHECK_EQ(value["test"].as<S>().double_member, 3.14);
  }
}
