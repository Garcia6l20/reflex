#include <doctest/doctest.h>

import reflex.serde.json;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

#define JSON(...) #__VA_ARGS__

struct[[= serde::naming::camel_case]] S
{
  int                                    int_member;
  std::string                            string_member;
  [[= serde::naming::kebab_case]] double double_member;

  constexpr bool operator==(S const& other) const = default;
};

struct[[= serde::naming::camel_case]] S2
{
  int                         i;
  std::array<char, 5>         chars;
  std::array<std::uint8_t, 5> nums;

  constexpr bool operator==(S2 const& other) const = default;
};

enum class[[= derive(Format, Parse)]] Color
{
  Red,
  Green,
  Blue
};

enum class[[= derive(EnumFlags, Format, Parse)]] FilePermissions
{
  None    = 0,
  Read    = 1 << 0,
  Write   = 1 << 1,
  Execute = 1 << 2
};

using namespace std::string_view_literals;

TEST_CASE("reflex::serde::json::serializer: base types")
{
  std::ostringstream out;
  json::serializer   ser{out};

  using json::null;

  SUBCASE("null")
  {
    ser.dump(null);
    CHECK_EQ(out.str(), "null"sv);
  }

  SUBCASE("string")
  {
    ser.dump("Hello, world!");
    CHECK_EQ(out.str(), "\"Hello, world!\"");
  }

  SUBCASE("number")
  {
    ser.dump(42);
    CHECK_EQ(out.str(), "42");
  }

  SUBCASE("boolean")
  {
    ser.dump(true);
    CHECK_EQ(out.str(), "true");
    out.str("");
    ser.dump(false);
    CHECK_EQ(out.str(), "false");
  }

  SUBCASE("enum")
  {
    ser.dump(Color::Green);
    CHECK_EQ(out.str(), "\"Green\"");
  }

  SUBCASE("enum-flags")
  {
    constexpr auto perms = FilePermissions::Read | FilePermissions::Write;
    ser.dump(perms);
    CHECK_EQ(out.str(), "\"Read|Write\"");
  }

  SUBCASE("char array")
  {
    std::array<char, 5> arr = {'H', 'e', 'l', 'l', 'o'};
    ser.dump(arr);
    std::println("Serialized char array: {}", out.str());
    CHECK_EQ(out.str(), "\"Hello\"");
  }

  SUBCASE("num array")
  {
    std::array<std::uint8_t, 5> arr = {0x01, 0x02, 0x03, 0x04, 0x05};
    ser.dump(arr);
    std::println("Serialized num array: {}", out.str());
    CHECK_EQ(out.str(), "[1,2,3,4,5]");
  }
}

TEST_CASE("reflex::serde::json::serializer: sequence and map")
{
  std::string      out;
  json::serializer ser{out};
  SUBCASE("sequence")
  {
    std::vector<int> arr = {1, 2, 3};
    ser.dump(arr);
    CHECK_EQ(out, JSON([1,2,3]));
  }

  SUBCASE("map")
  {
    auto obj = std::map<std::string, int>{
        {"a", 1},
        {"b", 2}
    };
    ser.dump(obj);
    CHECK_EQ(out, JSON({"a":1,"b":2}));
  }
}

TEST_CASE("reflex::serde::json::serializer: aggregate")
{
  std::string out;
  out.reserve(128); // avoid reallocations during serialization
  json::serializer ser{out};

  SUBCASE("simple aggregate")
  {
    S s{42, "Hello, world!", 3.14};
    ser.dump(s);
    CHECK_EQ(out, JSON({"intMember":42,"stringMember":"Hello, world!","double-member":3.14}));
  }

  SUBCASE("complex aggregate")
  {
    S2 s2{
        42, {'H', 'e', 'l', 'l', 'o'},
         {1,   2,   3,   4,   5  }
    };
    ser.dump(s2);
    CHECK_EQ(out, JSON({"i":42,"chars":"Hello","nums":[1,2,3,4,5]}));
  }
}

TEST_CASE("reflex::serde::json::serializer: nested aggregate")
{
  std::string out;
  out.reserve(128); // avoid reallocations during serialization
  json::serializer ser{out};

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
  ser.dump(o);
  CHECK_EQ(out, JSON({"inner":{"intMember":42},"stringMember":"Hello, world!"}));
}

TEST_CASE("reflex::serde::json::deserializer: base types")
{
  using json::null;
  SUBCASE("null")
  {
    const auto in    = "null"s;
    auto       value = json::deserializer{in}.load<json::null_t>();
    CHECK(value == null);
    CHECK(json::deserializer{in}.load<json::value>().is_null());
  }

  SUBCASE("string")
  {
    const std::string_view in    = JSON("Hello, world!");
    auto                   value = json::deserializer{in}.load<std::string>();
    CHECK(value == "Hello, world!");
    auto var = json::deserializer{in}.load<json::value>();
    CHECK(var.is<json::string>());
    CHECK(var == "Hello, world!");
  }

  SUBCASE("number")
  {
    const std::string_view in    = JSON(42);
    auto                   value = json::deserializer{in}.load<int>();
    CHECK(value == 42);
    auto var = json::deserializer{in}.load<json::value>();
    CHECK(var.is<json::number>());
    CHECK(var == 42);
  }

  SUBCASE("boolean")
  {
    const std::string_view in    = JSON(true);
    auto                   value = json::deserializer{in}.load<bool>();
    CHECK(value == true);
    const std::string_view in_false = JSON(false);
    value                           = json::deserializer{in_false}.load<bool>();
    CHECK(value == false);
  }

  SUBCASE("enum")
  {
    const std::string_view in    = JSON("Green");
    auto                   value = json::deserializer{in}.load<Color>();
    CHECK(value == Color::Green);
  }

  SUBCASE("enum-flags")
  {
    const std::string_view in    = JSON("Read|Write");
    auto                   value = json::deserializer{in}.load<FilePermissions>();
    CHECK((value & FilePermissions::Read) == FilePermissions::Read);
    CHECK((value & FilePermissions::Write) == FilePermissions::Write);
    CHECK((value & FilePermissions::Execute) == FilePermissions::None);
  }

  SUBCASE("char array")
  {
    const std::string_view in    = JSON("Hello");
    auto                   value = json::deserializer{in}.load<std::array<char, 5>>();
    CHECK(value == std::array<char, 5>{'H', 'e', 'l', 'l', 'o'});
  }

  SUBCASE("num array")
  {
    const std::string_view in    = JSON([1,2,3,4,5]);
    auto                   value = json::deserializer{in}.load<std::array<std::uint8_t, 5>>();
    CHECK(value == std::array<std::uint8_t, 5>{1, 2, 3, 4, 5});
  }
}

TEST_CASE("reflex::serde::json::deserializer: sequence and map")
{
  SUBCASE("sequence")
  {
    const std::string_view in    = JSON([1,2,3]);
    const auto             value = json::deserializer{in}.load<json::array>();
    CHECK_EQ(value.size(), 3);
    CHECK_EQ(value[0], 1);
    CHECK_EQ(value[1], 2);
    CHECK_EQ(value[2], 3);
    const auto expected = json::array{1, 2, 3};
    CHECK_EQ(value, expected);
  }

  SUBCASE("map")
  {
    const std::string_view in    = JSON({"a":1,"b":2});
    const auto             value = json::deserializer{in}.load<json::object>();
    std::println("Parsed object: {}", value);
    CHECK_EQ(value.size(), 2);
    CHECK_EQ(value.at("a"), 1);
    CHECK_EQ(value.at("b"), 2);
    const auto expected = json::object{
        {"a", 1},
        {"b", 2}
    };
    CHECK_EQ(value, expected);
  }
}

TEST_CASE("reflex::serde::json::deserializer: aggregate")
{
  static_assert(serde::object_visitable_c<S>);
  static_assert(serde::object_visitable_c<S&>);

  SUBCASE("direct load")
  {
    const std::string_view in =
        JSON({"intMember":42,"stringMember":"Hello, world!","double-member":3.14});
    const auto value = json::deserializer{in}.load<S>();
    CHECK_EQ(value.int_member, 42);
    CHECK_EQ(value.string_member, "Hello, world!");
    CHECK_EQ(value.double_member, 3.14);
  }
  SUBCASE("complex aggregate")
  {
    const std::string_view in    = JSON({"i":42,"chars":"Hello","nums":[1,2,3,4,5]});
    const auto             value = json::deserializer{in}.load<S2>();
    std::println("Parsed S2: {}", debug(value));
    CHECK_EQ(value.i, 42);
    CHECK_EQ(value.chars, std::array<char, 5>{'H', 'e', 'l', 'l', 'o'});
    CHECK_EQ(value.nums, std::array<std::uint8_t, 5>{1, 2, 3, 4, 5});
  }
}

using custom_var1 = poly::var<json::string, json::number, json::boolean, S>;

TEST_CASE("reflex::serde::json::deserializer: custom var")
{
  std::string      out;
  json::serializer ser{out};

  SUBCASE("custom var")
  {
    serialize(
        ser, custom_var1{
                 {"test", S{42, "Hello, world!", 3.14}}
    });
    std::println("Serialized: {}", out);
    const auto value = json::deserializer{out}.load<custom_var1>();
    std::println("Deserialized: {}", value);
    CHECK_EQ(value.at("test")->as<S>().int_member, 42);
    CHECK_EQ(value.at("test")->as<S>().string_member, "Hello, world!");
    CHECK_EQ(value.at("test")->as<S>().double_member, 3.14);
  }
}

TEST_CASE("reflex::core::json file roundtrip")
{
  const std::filesystem::path json_path = "test.json";
  const S                     expected  = {42, "Hello, world!", 3.14};
  {
    std::ofstream    out_file{json_path};
    json::serializer ser{out_file};
    ser.dump(expected);
  }

  {
    std::ifstream in_file{json_path};
    const auto    value = json::deserializer{in_file}.load<S>();
    CHECK_EQ(value, expected);
  }

  std::filesystem::remove(json_path);
}
