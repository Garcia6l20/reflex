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
};

TEST_CASE("reflex::serde::bson: base types round-trip")
{
  bson::serializer       serializer;
  std::vector<std::byte> out;

  using bson::null;

  SUBCASE("null")
  {
    serializer(out, null);
    auto value = bson::deserializer::load<bson::null_t>(out);
    CHECK(value == null);
  }

  SUBCASE("string")
  {
    serializer(out, std::string{"Hello, world!"});
    auto value = bson::deserializer::load<std::string>(out);
    CHECK_EQ(value, "Hello, world!");
  }

  SUBCASE("int32")
  {
    serializer(out, 42);
    auto value = bson::deserializer::load<int>(out);
    CHECK_EQ(value, 42);
  }

  SUBCASE("int64")
  {
    serializer(out, (1ll << 40));
    auto value = bson::deserializer::load<long long>(out);
    CHECK_EQ(value, (1ll << 40));
  }

  SUBCASE("double")
  {
    serializer(out, 3.14);
    auto value = bson::deserializer::load<double>(out);
    CHECK(value == doctest::Approx(3.14));
  }

  SUBCASE("boolean")
  {
    serializer(out, true);
    auto value = bson::deserializer::load<bool>(out);
    CHECK(value);
  }
}

TEST_CASE("reflex::serde::bson: sequence and map")
{
  bson::serializer serializer;

  SUBCASE("sequence")
  {
    std::vector<std::byte> out;
    std::vector<int>       arr = {1, 2, 3};

    serializer(out, arr);
    auto value = bson::deserializer::load<std::vector<int>>(out);

    CHECK_EQ(value.size(), 3);
    CHECK_EQ(value[0], 1);
    CHECK_EQ(value[1], 2);
    CHECK_EQ(value[2], 3);
  }

  SUBCASE("map")
  {
    std::vector<std::byte> out;
    auto                   obj = std::map<std::string, int>{{"a", 1}, {"b", 2}};

    serializer(out, obj);
    auto value = bson::deserializer::load<std::map<std::string, int>>(out);

    CHECK_EQ(value.size(), 2);
    CHECK_EQ(value.at("a"), 1);
    CHECK_EQ(value.at("b"), 2);
  }
}

TEST_CASE("reflex::serde::bson: aggregate")
{
  bson::serializer       serializer;
  std::vector<std::byte> out;

  S s{42, "Hello, world!", 3.14};
  serializer(out, s);
  auto value = bson::deserializer::load<S>(out);

  CHECK_EQ(value.int_member, 42);
  CHECK_EQ(value.string_member, "Hello, world!");
  CHECK(value.double_member == doctest::Approx(3.14));
}

TEST_CASE("reflex::serde::bson: explicit bson scalar types")
{
  bson::serializer       serializer;
  std::vector<std::byte> out;

  SUBCASE("bson::int32")
  {
    auto input = bson::int32{42};
    serializer(out, input);
    auto value = bson::deserializer::load<bson::int32>(out);
    CHECK_EQ(value, input);
  }

  SUBCASE("bson::int64")
  {
    auto input = bson::int64{1ll << 40};
    serializer(out, input);
    auto value = bson::deserializer::load<bson::int64>(out);
    CHECK_EQ(value, input);
  }

  SUBCASE("bson::decimal128")
  {
    bson::decimal128 input{
        .bytes = {
                  std::byte{0x01},
                  std::byte{0x23},
                  std::byte{0x45},
                  std::byte{0x67},
                  std::byte{0x89},
                  std::byte{0xAB},
                  std::byte{0xCD},
                  std::byte{0xEF},
                  std::byte{0x10},
                  std::byte{0x32},
                  std::byte{0x54},
                  std::byte{0x76},
                  std::byte{0x98},
                  std::byte{0xBA},
                  std::byte{0xDC},
                  std::byte{0xFE},
                  }
    };

    serializer(out, input);
    auto value = bson::deserializer::load<bson::decimal128>(out);
    CHECK_EQ(value, input);
  }

  SUBCASE("bson::datetime")
  {
    bson::datetime input{1'700'000'000'123ll};
    serializer(out, input);
    auto value = bson::deserializer::load<bson::datetime>(out);
    CHECK_EQ(value, input);
  }
}

TEST_CASE("reflex::serde::bson: value from map")
{
  bson::serializer       serializer;
  std::vector<std::byte> out;

  auto obj = std::map<std::string, int>{{"a", 1}, {"b", 2}};
  std::println("Input: {}", obj);
  serializer(out, obj);
  std::println("Serialized: {}", out);

  auto value = bson::deserializer::load(out);
  std::println("Deserialized: {}", value);

  CHECK(value.is_object());
  CHECK_EQ(value.as<bson::object>().size(), 2);
  CHECK_EQ(value.as<bson::object>().at("a"), 1);
  CHECK_EQ(value.as<bson::object>().at("b"), 2);
}

TEST_CASE("reflex::serde::bson: value preserves bson scalar types")
{
  bson::serializer       serializer;
  std::vector<std::byte> out;

  auto typed = std::map<std::string, bson::value>{
      {"i32",  bson::int32{7}                     },
      {"i64",  bson::int64{1ll << 40}             },
      {"d128",
       bson::decimal128{
           .bytes =
               {
                   std::byte{0x00},
                   std::byte{0x11},
                   std::byte{0x22},
                   std::byte{0x33},
                   std::byte{0x44},
                   std::byte{0x55},
                   std::byte{0x66},
                   std::byte{0x77},
                   std::byte{0x88},
                   std::byte{0x99},
                   std::byte{0xAA},
                   std::byte{0xBB},
                   std::byte{0xCC},
                   std::byte{0xDD},
                   std::byte{0xEE},
                   std::byte{0xFF},
               }}                                 },
      {"dt",   bson::datetime{1'701'234'567'890ll}},
  };

  serializer(out, typed);
  auto value = bson::deserializer::load(out);

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
  bson::serializer       serializer;
  std::vector<std::byte> out;

  SUBCASE("empty input")
  {
    std::vector<std::byte> input;
    CHECK_THROWS_AS((bson::deserializer::load<int>(input)), std::runtime_error);
  }

  SUBCASE("truncated document")
  {
    serializer(out, std::map<std::string, int>{{"a", 1}});
    out.pop_back();
    CHECK_THROWS_AS((bson::deserializer::load<std::map<std::string, int>>(out)), std::runtime_error);
  }

  SUBCASE("invalid type tag")
  {
    serializer(out, 123);
    out[4] = std::byte{0x7F};
    CHECK_THROWS_AS((bson::deserializer::load<int>(out)), std::runtime_error);
  }
}
