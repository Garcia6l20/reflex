#include <doctest/doctest.h>

import reflex.serde.bson;

import std;

using namespace reflex;
using namespace reflex::serde;

struct[[= serde::naming::camel_case]] S
{
  int                                    int_member;
  std::string                            string_member;
  [[= serde::naming::kebab_case]] double double_member;

  bool operator==(S const&) const = default;
};

enum class Color
{
  Red,
  Green,
  Blue
};

TEST_CASE("reflex::serde::bson: base types round-trip")
{
  using bson::null;

  SUBCASE("null")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(null);
    auto value = bson::deserializer{out}.load<bson::null_t>();
    CHECK(value == null);
  }

  SUBCASE("string")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(std::string{"Hello, world!"});
    auto value = bson::deserializer{out}.load<std::string>();
    CHECK_EQ(value, "Hello, world!");
  }

  SUBCASE("int32")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(42);
    auto value = bson::deserializer{out}.load<int>();
    CHECK_EQ(value, 42);
  }

  SUBCASE("int64")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(1ll << 40);
    auto value = bson::deserializer{out}.load<long long>();
    CHECK_EQ(value, (1ll << 40));
  }

  SUBCASE("double")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(3.14);
    auto value = bson::deserializer{out}.load<double>();
    CHECK(value == doctest::Approx(3.14));
  }

  SUBCASE("boolean")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(true);
    auto value = bson::deserializer{out}.load<bool>();
    CHECK(value);
  }

  SUBCASE("enum")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(Color::Green);
    auto value = bson::deserializer{out}.load<Color>();
    CHECK(value == Color::Green);
  }
}

TEST_CASE("reflex::serde::bson: sequence and map")
{
  SUBCASE("sequence")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    std::vector<int>       arr = {1, 2, 3};

    ser.dump(arr);
    auto value = bson::deserializer{out}.load<std::vector<int>>();

    CHECK_EQ(value.size(), 3);
    CHECK_EQ(value[0], 1);
    CHECK_EQ(value[1], 2);
    CHECK_EQ(value[2], 3);
  }

  SUBCASE("map")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    auto                   obj = std::map<std::string, int>{
        {"a", 1},
        {"b", 2}
    };

    ser.dump(obj);
    auto value = bson::deserializer{out}.load<std::map<std::string, int>>();

    CHECK_EQ(value.size(), 2);
    CHECK_EQ(value.at("a"), 1);
    CHECK_EQ(value.at("b"), 2);
  }
}

TEST_CASE("reflex::serde::bson: aggregate")
{
  std::vector<std::byte> out;
  bson::serializer       ser{out};

  S s{42, "Hello, world!", 3.14};
  ser.dump(s);
  auto value = bson::deserializer{out}.load<S>();

  CHECK_EQ(value.int_member, 42);
  CHECK_EQ(value.string_member, "Hello, world!");
  CHECK(value.double_member == doctest::Approx(3.14));
}

TEST_CASE("reflex::serde::bson: explicit bson scalar types")
{
  SUBCASE("bson::int32")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    auto                   input = bson::int32{42};
    ser.dump(input);
    auto value = bson::deserializer{out}.load<bson::int32>();
    CHECK_EQ(value, input);
  }

  SUBCASE("bson::int64")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    auto                   input = bson::int64{1ll << 40};
    ser.dump(input);
    auto value = bson::deserializer{out}.load<bson::int64>();
    CHECK_EQ(value, input);
  }

  SUBCASE("bson::decimal128")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    bson::decimal128       input{42.2};
    ser.dump(input);
    auto value = bson::deserializer{out}.load<bson::decimal128>();
    CHECK_EQ(value, input);
  }

  SUBCASE("bson::datetime")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    bson::datetime         input{1'700'000'000'123ll};
    ser.dump(input);
    auto value = bson::deserializer{out}.load<bson::datetime>();
    CHECK_EQ(value, input);
  }
}

TEST_CASE("reflex::serde::bson: value from map")
{
  std::vector<std::byte> out;
  bson::serializer       ser{out};

  auto obj = std::map<std::string, int>{
      {"a", 1},
      {"b", 2}
  };
  std::println("Input: {}", obj);
  ser.dump(obj);
  std::println("Serialized: {}", out);

  auto value = bson::deserializer{out}.load();
  std::println("Deserialized: {}", value);

  CHECK(value.is_object());
  CHECK_EQ(value.as<bson::object>().size(), 2);
  CHECK_EQ(value.as<bson::object>().at("a"), 1);
  CHECK_EQ(value.as<bson::object>().at("b"), 2);
}

TEST_CASE("reflex::serde::bson: value preserves bson scalar types")
{
  std::vector<std::byte> out;
  bson::serializer       ser{out};

  auto typed = std::map<std::string, bson::value>{
      {"i32",  bson::int32{7}                     },
      {"i64",  bson::int64{1ll << 40}             },
      {"d128", bson::decimal128{42.2}             },
      {"dt",   bson::datetime{1'701'234'567'890ll}},
  };

  ser.dump(typed);
  auto value = bson::deserializer{out}.load();

  CHECK(value.is_object());
  auto const& obj = value.as<bson::object>();

  CHECK(obj.at("i32").is<bson::int32>());
  CHECK_EQ(obj.at("i32").as<bson::int32>(), 7);

  CHECK(obj.at("i64").is<bson::int64>());
  CHECK_EQ(obj.at("i64").as<bson::int64>(), (1ll << 40));

  CHECK(obj.at("d128").is<bson::decimal128>());
  CHECK_EQ(obj.at("d128").as<bson::decimal128>(), typed.at("d128").as<bson::decimal128>());

  CHECK(obj.at("dt").is<bson::datetime>());
  CHECK_EQ(bson::datetime{1'701'234'567'890ll}, obj.at("dt").as<bson::datetime>());
}

TEST_CASE("reflex::serde::bson: malformed input throws")
{
  SUBCASE("empty input")
  {
    std::vector<std::byte> input;
    CHECK_THROWS_AS(
        (bson::deserializer{input.begin(), input.end()}.load<int>()), std::runtime_error);
  }

  SUBCASE("truncated document")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(
        std::map<std::string, int>{
            {"a", 1}
    });
    out.pop_back();
    CHECK_THROWS_AS(
        (bson::deserializer{out}.load<std::map<std::string, int>>()), std::runtime_error);
  }

  SUBCASE("invalid type tag")
  {
    std::vector<std::byte> out;
    bson::serializer       ser{out};
    ser.dump(123);
    out[4] = std::byte{0x7F};
    CHECK_THROWS_AS((bson::deserializer{out}.load<int>()), std::runtime_error);
  }
}

TEST_CASE("reflex::core::bson file roundtrip")
{
  const std::filesystem::path bson_path = "test.bson";
  const S                     expected  = {42, "Hello, world!", 3.14};

  {
    std::ofstream    out_file{bson_path, std::ios::binary};
    bson::serializer ser{out_file};
    ser.dump(expected);
  }

  {
    std::ifstream in_file{bson_path, std::ios::binary};
    const auto    value = bson::deserializer{in_file}.load<S>();
    CHECK_EQ(value, expected);
  }

  std::filesystem::remove(bson_path);
}
